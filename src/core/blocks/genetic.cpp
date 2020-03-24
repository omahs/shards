#ifndef GENETIC_H
#define GENETIC_H

#include "blockwrapper.hpp"
#include "chainblocks.h"
#include "chainblocks.hpp"
#include "random.hpp"
#include "shared.hpp"
#include <limits>
#include <sstream>
#include <taskflow/taskflow.hpp>

namespace chainblocks {
namespace Genetic {
struct GeneticRng;
using Rng = Rng<GeneticRng>;
struct Mutant;
struct Evolve {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return _outputType; }
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _baseChain = value;
      break;
    case 1:
      _fitnessChain = value;
      break;
    case 2:
      _popsize = value.payload.intValue;
      break;
    case 3:
      _mutation = value.payload.floatValue;
      break;
    case 4:
      _crossover = value.payload.floatValue;
      break;
    case 5:
      _extinction = value.payload.floatValue;
      break;
    case 6:
      _elitism = value.payload.floatValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _baseChain;
    case 1:
      return _fitnessChain;
    case 2:
      return Var(_popsize);
    case 3:
      return Var(_mutation);
    case 4:
      return Var(_crossover);
    case 5:
      return Var(_extinction);
    case 6:
      return Var(_elitism);
    default:
      return CBVar();
    }
  }

  struct Writer {
    std::stringstream &_stream;
    Writer(std::stringstream &stream) : _stream(stream) {}
    void operator()(const uint8_t *buf, size_t size) {
      _stream.write((const char *)buf, size);
    }
  };

  struct Reader {
    std::stringstream &_stream;
    Reader(std::stringstream &stream) : _stream(stream) {}
    void operator()(uint8_t *buf, size_t size) {
      _stream.read((char *)buf, size);
    }
  };

  void cleanup() {
    if (_population.size() > 0) {
      tf::Taskflow cleanupFlow;
      cleanupFlow.parallel_for(
          _population.begin(), _population.end(), [&](Individual &i) {
            // Free and release chain
            auto chain = CBChain::sharedFromRef(i.chain.payload.chainValue);
            stop(chain.get());
            Serialization::varFree(i.chain);
          });
      Tasks.run(cleanupFlow).get();

      _chain2Individual.clear();
      _sortedPopulation.clear();
      _population.clear();
    }
    _baseChain.cleanup();
  }

  void warmup(CBContext *ctx) { _baseChain.warmup(ctx); }

  struct TickObserver {
    Evolve &self;

    void before_tick(CBChain *chain) {}
    void before_start(CBChain *chain) {}
    void before_prepare(CBChain *chain) {}

    void before_stop(CBChain *chain) {
      const auto individualIt = self._chain2Individual.find(chain);
      if (individualIt == self._chain2Individual.end()) {
        assert(false);
      }
      auto &individual = individualIt->second.get();
      // Collect fitness last result
      auto fitnessVar = chain->previousOutput;
      if (fitnessVar.valueType == Float) {
        individual.fitness = fitnessVar.payload.floatValue;
      }
    }

    void before_compose(CBChain *chain) {
      // Do mutations
      const auto individualIt = self._chain2Individual.find(chain);
      if (individualIt == self._chain2Individual.end()) {
        assert(false);
      }
      auto &individual = individualIt->second.get();
      // Keep in mind that individuals might not reach chain termination
      // they might "crash", so fitness should be set to minimum before any run
      individual.fitness = std::numeric_limits<double>::min();
    }
  };

  CBVar activate(CBContext *context, const CBVar &input) {
    AsyncOp<InternalCore> op(context);
    return op([&]() {
      // Init on the first run!
      // We reuse those chains for every era
      // Only the DNA changes
      if (_population.size() == 0) {
        std::stringstream chainStream;
        Writer w1(chainStream);
        Serialization::serialize(_baseChain, w1);
        auto chainStr = chainStream.str();

        std::stringstream fitnessStream;
        Writer w2(fitnessStream);
        Serialization::serialize(_fitnessChain, w2);
        auto fitnessStr = fitnessStream.str();

        _population.resize(_popsize);
        _nkills = size_t(double(_popsize) * _extinction);
        _nelites = size_t(double(_popsize) * _elitism);

        tf::Taskflow initFlow;
        initFlow.parallel_for(
            _population.begin(), _population.end(), [&](Individual &i) {
              std::stringstream i1Stream(chainStr);
              Reader r1(i1Stream);
              Serialization::deserialize(r1, i.chain);
              auto chain = CBChain::sharedFromRef(i.chain.payload.chainValue);
              gatherMutants(chain.get(), i.mutants);

              std::stringstream i2Stream(fitnessStr);
              Reader r2(i2Stream);
              Serialization::deserialize(r2, i.fitnessChain);
            });
        Tasks.run(initFlow).get();

        // Also populate chain2Indi
        for (auto &i : _population) {
          auto fitchain =
              CBChain::sharedFromRef(i.fitnessChain.payload.chainValue);
          _chain2Individual.emplace(fitchain.get(), i);
          _sortedPopulation.emplace_back(i);
        }
      } else {
        // do crossover here, populating tasks properly
        tf::Taskflow crossoverFlow;
        _crossingOver.clear();
        int currentIdx = 0;
        for (auto &ind : _sortedPopulation) {
          if (Rng::frand() < _crossover) {
            // In this case this individual
            // becomes the child between two other individuals
            // there is a chance also to keep current values
            // so this is effectively tree way crossover
            // Select from high fitness individuals
            const auto parent0Idx =
                int(std::pow(Rng::frand(), 4) * double(_popsize));
            auto parent0 = _sortedPopulation[parent0Idx];
            const auto parent1Idx =
                int(std::pow(Rng::frand(), 4) * double(_popsize));
            auto parent1 = _sortedPopulation[parent1Idx];
            if (currentIdx != parent0Idx && currentIdx != parent1Idx &&
                parent0.get().parent0Idx != currentIdx &&
                parent0.get().parent1Idx != currentIdx &&
                parent1.get().parent0Idx != currentIdx &&
                parent1.get().parent1Idx != currentIdx) {
              ind.get().crossoverTask = crossoverFlow.emplace(
                  [=]() { crossover(ind, parent0, parent1); });
              _crossingOver.emplace_back(ind, parent0, parent1);
            }
          } else {
            ind.get().crossoverTask.reset();
          }
          currentIdx++;
        }
        for (auto [a, b, c] : _crossingOver) {
          auto &atask = a.get().crossoverTask;
          auto &btask = b.get().crossoverTask;
          auto &ctask = c.get().crossoverTask;
          if (!btask.empty())
            btask.precede(atask);
          if (!ctask.empty())
            ctask.precede(atask);
        }
        Tasks.run(crossoverFlow).get();
      }

      // We run chains up to completion
      // From validation to end, every iteration/era
      {
        tf::Taskflow runFlow;
        runFlow.parallel_for(
            _population.begin(), _population.end(), [&](Individual &i) {
              CBNode node;
              TickObserver obs{*this};

              // Evaluate our brain chain
              auto chain = CBChain::sharedFromRef(i.chain.payload.chainValue);
              node.schedule(chain.get());
              while (!node.empty()) {
                node.tick();
              }

              // compute the fitness
              auto fitchain =
                  CBChain::sharedFromRef(i.fitnessChain.payload.chainValue);
              node.schedule(obs, fitchain.get(), chain->previousOutput);
              while (!node.empty()) {
                node.tick(obs);
              }
            });
        Tasks.run(runFlow).get();
      }

      std::sort(_sortedPopulation.begin(), _sortedPopulation.end(),
                [](std::reference_wrapper<Individual> a,
                   std::reference_wrapper<Individual> b) {
                  return a.get().fitness > b.get().fitness;
                });

      // reset flags
      std::for_each(_sortedPopulation.begin(),
                    _sortedPopulation.end() - _nkills, [](auto &i) {
                      auto &id = i.get();
                      id.extinct = false;
                      id.parent0Idx = -1;
                      id.parent1Idx = -1;
                    });
      std::for_each(_sortedPopulation.end() - _nkills, _sortedPopulation.end(),
                    [](auto &i) {
                      auto &id = i.get();
                      id.extinct = true;
                      id.parent0Idx = -1;
                      id.parent1Idx = -1;
                    });

      // Do mutations at end, yet when contexts are still valid!
      // since we might need them
      {
        tf::Taskflow mutFlow;
        mutFlow.parallel_for(_sortedPopulation.begin() + _nelites,
                             _sortedPopulation.end(), [&](auto &i) {
                               // reset the individual if extinct
                               auto &individual = i.get();
                               if (individual.extinct) {
                                 resetState(individual);
                               }
                               mutate(individual);
                             });
        Tasks.run(mutFlow).get();
      }

      _result.clear();
      _result.emplace_back(Var(_sortedPopulation.front().get().fitness));
      _result.emplace_back(_sortedPopulation.front().get().chain);
      return Var(_result);
    });
  }

private:
  struct MutantInfo {
    MutantInfo(const Mutant &block) : block(block) {}

    std::reference_wrapper<const Mutant> block;

    // For reset... hmm we copy them due to multi thread
    // ideally they are all the same tho..
    std::vector<std::tuple<int, OwnedVar>> originalParams;
  };

  static inline void gatherMutants(CBChain *chain,
                                   std::vector<MutantInfo> &out);

  struct Individual {
    // Chains are recycled
    CBVar chain{};
    // We need many of them cos we use threads
    CBVar fitnessChain{};

    // Keep track of mutants and push/pop mutations on chain
    std::vector<MutantInfo> mutants;

    double fitness{std::numeric_limits<double>::min()};

    bool extinct = false;

    tf::Task crossoverTask;
    int parent0Idx = -1;
    int parent1Idx = -1;
  };

  inline void crossover(Individual &child, const Individual &parent0,
                        const Individual &parent1);
  inline void mutate(Individual &individual);
  inline void resetState(Individual &individual);

  static inline Parameters _params{
      {"Chain",
       "The chain to optimize and evolve.",
       {CoreInfo::ChainType, CoreInfo::ChainVarType}},
      {"Fitness",
       "The fitness chain to run at the end of the main chain evaluation and "
       "using "
       "its last output; should output a Float fitness value.",
       {CoreInfo::ChainType, CoreInfo::ChainVarType}},
      {"Population", "The population size.", {CoreInfo::IntType}},
      {"Mutation", "The rate of mutation, 0.1 = 10%.", {CoreInfo::FloatType}},
      {"Crossover", "The rate of crossover, 0.1 = 10%.", {CoreInfo::FloatType}},
      {"Extinction",
       "The rate of extinction, 0.1 = 10%.",
       {CoreInfo::FloatType}},
      {"Elitism", "The rate of elitism, 0.1 = 10%.", {CoreInfo::FloatType}}};
  static inline Types _outputTypes{{CoreInfo::FloatType, CoreInfo::ChainType}};
  static inline Type _outputType{{CBType::Seq, {.seqTypes = _outputTypes}}};

  tf::Executor &Tasks{Singleton<tf::Executor>::value};

  ParamVar _baseChain{};
  ParamVar _fitnessChain{};
  std::vector<CBVar> _result;
  std::vector<Individual> _population;
  std::vector<std::reference_wrapper<Individual>> _sortedPopulation;
  std::vector<std::tuple<std::reference_wrapper<Individual>,
                         std::reference_wrapper<Individual>,
                         std::reference_wrapper<Individual>>>
      _crossingOver;
  std::unordered_map<CBChain *, std::reference_wrapper<Individual>>
      _chain2Individual;
  int64_t _popsize = 64;
  double _mutation = 0.2;
  double _crossover = 0.2;
  double _extinction = 0.1;
  size_t _nkills = 0;
  double _elitism = 0.1;
  size_t _nelites = 0;
};

struct Mutant {
  CBTypesInfo inputTypes() {
    if (_block) {
      auto blks = _block.blocks();
      return blks.elements[0]->inputTypes(blks.elements[0]);
    } else {
      return CoreInfo::AnyType;
    }
  }

  CBTypesInfo outputTypes() {
    if (_block) {
      auto blks = _block.blocks();
      return blks.elements[0]->outputTypes(blks.elements[0]);
    } else {
      return CoreInfo::AnyType;
    }
  }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _block = value;
      break;
    case 1:
      _indices = value;
      break;
    case 2: {
      destroyBlocks();
      _mutations = value;
      if (_mutations.valueType == Seq) {
        IterableSeq muts(_mutations);
        for (auto &mut : muts) {
          if (mut.valueType == Block) {
            auto blk = mut.payload.blockValue;
            blk->owned = true;
          } else if (mut.valueType == Seq) {
            IterableSeq blks(mut);
            for (auto &bv : blks) {
              auto blk = bv.payload.blockValue;
              blk->owned = true;
            }
          }
        }
      }
    } break;
    case 3:
      _options = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _block;
    case 1:
      return _indices;
    case 2:
      return _mutations;
    case 3:
      return _options;
    default:
      return CBVar();
    }
  }

  void cleanupMutations() {
    if (_mutations.valueType == Seq) {
      IterableSeq muts(_mutations);
      for (auto &mut : muts) {
        if (mut.valueType == Block) {
          auto blk = mut.payload.blockValue;
          blk->cleanup(blk);
        } else if (mut.valueType == Seq) {
          IterableSeq blks(mut);
          for (auto &bv : blks) {
            auto blk = bv.payload.blockValue;
            blk->cleanup(blk);
          }
        }
      }
    }
  }

  void cleanup() {
    _block.cleanup();
    cleanupMutations();
  }

  void destroyBlocks() {
    if (_mutations.valueType == Seq) {
      IterableSeq muts(_mutations);
      for (auto &mut : muts) {
        if (mut.valueType == Block) {
          auto blk = mut.payload.blockValue;
          blk->cleanup(blk);
          blk->destroy(blk);
        } else if (mut.valueType == Seq) {
          IterableSeq blks(mut);
          for (auto &bv : blks) {
            auto blk = bv.payload.blockValue;
            blk->cleanup(blk);
            blk->destroy(blk);
          }
        }
      }
    }
  }

  void destroy() { destroyBlocks(); }

  void warmup(CBContext *ctx) {
    _block.warmup(ctx);

    if (_mutations.valueType == Seq) {
      IterableSeq muts(_mutations);
      for (auto &mut : muts) {
        if (mut.valueType == Block) {
          auto blk = mut.payload.blockValue;
          if (blk->warmup)
            blk->warmup(blk, ctx);
        } else if (mut.valueType == Seq) {
          IterableSeq blks(mut);
          for (auto &bv : blks) {
            auto blk = bv.payload.blockValue;
            if (blk->warmup)
              blk->warmup(blk, ctx);
          }
        }
      }
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    auto inner = mutant();
    if (_mutations.valueType == Seq && inner) {
      auto dataCopy = data;
      IterableSeq muts(_mutations);
      int idx = 0;
      auto innerParams = inner->parameters(inner);
      for (auto &mut : muts) {
        if (idx >= int(innerParams.len))
          break;
        ToTypeInfo ptype(inner->getParam(inner, idx));
        dataCopy.inputType = ptype;
        if (mut.valueType == Block) {
          auto blk = mut.payload.blockValue;
          if (blk->compose) {
            auto res = blk->compose(blk, dataCopy);
            if (res != ptype) {
              throw CBException("Expected same type as input in parameter "
                                "mutation chain's output.");
            }
          }
        } else if (mut.valueType == Seq) {
          auto res = validateConnections(
              mut.payload.seqValue,
              [](const CBlock *errorBlock, const char *errorTxt,
                 bool nonfatalWarning, void *userData) {},
              nullptr, dataCopy);
          if (res.outputType != ptype) {
            throw CBException("Expected same type as input in parameter "
                              "mutation chain's output.");
          }
          arrayFree(res.exposedInfo);
        }
        idx++;
      }
    }

    return _block.compose(data).outputType;
  }

  CBExposedTypesInfo exposedVariables() {
    if (!_block)
      return {};

    auto blks = _block.blocks();
    return blks.elements[0]->exposedVariables(blks.elements[0]);
  }

  CBExposedTypesInfo requiredVariables() {
    if (!_block)
      return {};

    auto blks = _block.blocks();
    return blks.elements[0]->exposedVariables(blks.elements[0]);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    return _block.activate(context, input);
  }

  CBlock *mutant() const {
    if (!_block)
      return nullptr;

    auto blks = _block.blocks();
    return blks.elements[0];
  }

private:
  friend struct Evolve;
  BlocksVar _block{};
  OwnedVar _indices{};
  OwnedVar _mutations{};
  OwnedVar _options{};
  static inline Parameters _params{
      {"Block", "The block to mutate.", {CoreInfo::BlockType}},
      {"Indices",
       "The parameter indices to mutate of the inner block.",
       {CoreInfo::IntSeqType}},
      {"Mutations",
       "Optional chains of blocks (or Nones) to call when mutating one of the "
       "parameters, if empty a default operator will be used.",
       {CoreInfo::BlocksOrNoneSeq}},
      {"Options",
       "Mutation options table - a table with mutation options.",
       {CoreInfo::NoneType, CoreInfo::AnyTableType}}};
};

void registerBlocks() {
  REGISTER_CBLOCK("Evolve", Evolve);
  REGISTER_CBLOCK("Mutant", Mutant);
}

inline void mutateVar(CBVar &var) {
  switch (var.valueType) {
  case CBType::Int: {
    const auto add = Rng::fnormal(0.0, 1.0) * double(var.payload.intValue);
    var.payload.intValue += int64_t(add);
  } break;
  case CBType::Int2: {
    for (auto i = 0; i < 2; i++) {
      const auto add =
          Rng::fnormal(0.0, 1.0) * double(var.payload.int2Value[i]);
      var.payload.int2Value[i] += int64_t(add);
    }
  } break;
  case CBType::Int3: {
    for (auto i = 0; i < 3; i++) {
      const auto add =
          Rng::fnormal(0.0, 1.0) * double(var.payload.int3Value[i]);
      var.payload.int3Value[i] += int32_t(add);
    }
  } break;
  case CBType::Int4: {
    for (auto i = 0; i < 4; i++) {
      const auto add =
          Rng::fnormal(0.0, 1.0) * double(var.payload.int4Value[i]);
      var.payload.int4Value[i] += int32_t(add);
    }
  } break;
  case CBType::Int8: {
    for (auto i = 0; i < 8; i++) {
      const auto add =
          Rng::fnormal(0.0, 1.0) * double(var.payload.int8Value[i]);
      var.payload.int8Value[i] += int16_t(add);
    }
  } break;
  case CBType::Int16: {
    for (auto i = 0; i < 16; i++) {
      const auto add =
          Rng::fnormal(0.0, 1.0) * double(var.payload.int16Value[i]);
      var.payload.int16Value[i] += int8_t(add);
    }
  } break;
  case CBType::Float: {
    var.payload.floatValue += Rng::fnormal(0.0, 0.1);
  } break;
  case CBType::Float2: {
    for (auto i = 0; i < 2; i++)
      var.payload.float2Value[i] += Rng::fnormal(0.0, 0.1);
  } break;
  case CBType::Float3: {
    for (auto i = 0; i < 3; i++)
      var.payload.float3Value[i] += float(Rng::fnormal(0.0, 0.1));
  } break;
  case CBType::Float4: {
    for (auto i = 0; i < 4; i++)
      var.payload.float4Value[i] += float(Rng::fnormal(0.0, 0.1));
  } break;
  default:
    break;
  }
}

inline void Evolve::gatherMutants(CBChain *chain,
                                  std::vector<MutantInfo> &out) {
  std::vector<CBlockInfo> blocks;
  gatherBlocks(chain, blocks);
  auto pos =
      std::remove_if(std::begin(blocks), std::end(blocks),
                     [](CBlockInfo &info) { return info.name != "Mutant"; });
  if (pos != std::end(blocks)) {
    std::for_each(std::begin(blocks), pos, [&](CBlockInfo &info) {
      auto mutator = reinterpret_cast<const BlockWrapper<Mutant> *>(info.block);
      auto &minfo = out.emplace_back(mutator->block);
      auto mutant = mutator->block.mutant();
      if (mutator->block._indices.valueType == Seq) {
        IterableSeq indices(mutator->block._indices);
        for (auto &idx : indices) {
          auto i = int(idx.payload.intValue);
          minfo.originalParams.emplace_back(i, mutant->getParam(mutant, i));
        }
      }
    });
  }
}

inline void Evolve::crossover(Individual &child, const Individual &parent0,
                              const Individual &parent1) {
  auto cmuts = child.mutants.cbegin();
  auto p0muts = parent0.mutants.cbegin();
  auto p1muts = parent1.mutants.cbegin();
  for (; cmuts != child.mutants.end() && p0muts != parent0.mutants.end() &&
         p1muts != parent1.mutants.end();
       ++cmuts, ++p0muts, ++p1muts) {
    auto cb = cmuts->block.get().mutant();
    auto p0b = p0muts->block.get().mutant();
    auto p1b = p1muts->block.get().mutant();
    if (cb && p0b && p1b) {
      // use crossover proc if avail
      if (cb->crossover) {
        auto p0s = p0b->getState(p0b);
        auto p1s = p1b->getState(p1b);
        cb->crossover(cb, p0s, p1s);
      }
      // check if we have mutant params and cross them over
      auto &indices = cmuts->block.get()._indices;
      if (indices.valueType == Seq) {
        IterableSeq idxs(indices);
        for (auto &idx : idxs) {
          const auto i = int(idx.payload.intValue);
          const auto r = Rng::frand();
          if (r < 0.33) {
            // take from 0
            auto val = p0b->getParam(p0b, i);
            cb->setParam(cb, i, val);
          } else if (r < 0.66) {
            // take from 1
            auto val = p1b->getParam(p1b, i);
            cb->setParam(cb, i, val);
          } // else we keep
        }
      }
    }
  }
}

inline void Evolve::mutate(Evolve::Individual &individual) {
  auto chain = CBChain::sharedFromRef(individual.chain.payload.chainValue);
  // we need to hack this in as we run out of context
  CBCoro foo{};
  CBContext ctx(std::move(foo), chain.get());
  ctx.chainStack.push_back(chain.get());
  std::for_each(
      std::begin(individual.mutants), std::end(individual.mutants),
      [&](MutantInfo &info) {
        if (Rng::frand() > _mutation) {
          return;
        }

        auto &mutator = info.block.get();
        auto &options = mutator._options;
        if (info.block.get().mutant()) {
          auto mutant = info.block.get().mutant();
          auto &indices = mutator._indices;
          if (mutant->mutate && (indices.valueType == None || rand() < 0.5)) {
            // In the case the block has `mutate`
            auto table = options.valueType == Table ? options.payload.tableValue
                                                    : CBTable();
            mutant->mutate(mutant, table);
          } else if (indices.valueType == Seq) {
            auto &iseq = indices.payload.seqValue;
            // do stuff on the param
            // select a random one
            auto rparam = Rng::rand(iseq.len);
            auto current = mutant->getParam(
                mutant, int(iseq.elements[rparam].payload.intValue));
            // if we have mutation blocks use them
            // if not use default operation
            if (mutator._mutations.valueType == Seq &&
                uint32_t(rparam) < mutator._mutations.payload.seqValue.len) {
              auto mblks = mutator._mutations.payload.seqValue.elements[rparam];
              if (mblks.valueType == Block) {
                auto blk = mblks.payload.blockValue;
                current = blk->activate(blk, &ctx, &current);
              } else if (mblks.valueType == Seq) {
                auto blks = mblks.payload.seqValue;
                CBVar out{};
                activateBlocks(blks, &ctx, current, out);
                current = out;
              } else {
                // Was likely None, so use default op
                mutateVar(current);
              }
            } else {
              mutateVar(current);
            }
            mutant->setParam(
                mutant, int(iseq.elements[rparam].payload.intValue), current);
          }
        }
      });
}

inline void Evolve::resetState(Evolve::Individual &individual) {
  // Reset params and state
  std::for_each(std::begin(individual.mutants), std::end(individual.mutants),
                [](MutantInfo &info) {
                  auto mutant = info.block.get().mutant();
                  if (!mutant)
                    return;

                  if (mutant->resetState)
                    mutant->resetState(mutant);

                  for (auto [idx, val] : info.originalParams) {
                    mutant->setParam(mutant, idx, val);
                  }
                });
}
} // namespace Genetic
} // namespace chainblocks

#endif /* GENETIC_H */