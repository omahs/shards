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
struct Mutant;
struct Evolve {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::FloatType; }
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _baseChain = value;
      break;
    case 1:
      _popsize = value.payload.intValue;
      break;
    case 2:
      _mutation = value.payload.floatValue;
      break;
    case 3:
      _extinction = value.payload.floatValue;
      break;
    case 4:
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
      return Var(_popsize);
    case 2:
      return Var(_mutation);
    case 3:
      return Var(_extinction);
    case 4:
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

      self.saveState(individual);
    }

    void before_compose(CBChain *chain) {
      // Do mutations
      const auto individualIt = self._chain2Individual.find(chain);
      if (individualIt == self._chain2Individual.end()) {
        assert(false);
      }
      auto &individual = individualIt->second.get();
      // Restore state if not killed
      // Else reset the individual
      if (!individual.extinct) {
        self.restoreState(individual);
      } else {
        self.resetState(individual);
      }
      // Call mutate if not elite
      if (!individual.elite) {
        self.mutate(individual);
      }
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
        Writer w(chainStream);
        Serialization::serialize(_baseChain, w);
        auto chainStr = chainStream.str();
        _population.resize(_popsize);
        _nkills = size_t(double(_popsize) * _extinction);
        _nelites = size_t(double(_popsize) * _elitism);
        tf::Taskflow initFlow;
        initFlow.parallel_for(
            _population.begin(), _population.end(), [&](Individual &i) {
              std::stringstream inputStream(chainStr);
              Reader r(inputStream);
              Serialization::deserialize(r, i.chain);
              auto chain = CBChain::sharedFromRef(i.chain.payload.chainValue);
              gatherMutants(chain.get(), i.mutants);
            });
        Tasks.run(initFlow).get();

        // Also populate chain2Indi
        for (auto &i : _population) {
          auto chain = CBChain::sharedFromRef(i.chain.payload.chainValue);
          _chain2Individual.emplace(chain.get(), i);
          _sortedPopulation.emplace_back(i);
        }
      }
      // We run chains up to completion
      // From validation to end, every iteration/era
      tf::Taskflow runFlow;
      runFlow.parallel_for(
          _population.begin(), _population.end(), [&](Individual &i) {
            CBNode node;
            TickObserver obs{*this};
            auto chain = CBChain::sharedFromRef(i.chain.payload.chainValue);
            node.schedule(obs, chain.get());
            while (!node.empty()) {
              node.tick(obs);
            }
          });
      Tasks.run(runFlow).get();

      std::sort(_sortedPopulation.begin(), _sortedPopulation.end(),
                [](std::reference_wrapper<Individual> a,
                   std::reference_wrapper<Individual> b) {
                  return a.get().fitness > b.get().fitness;
                });

      // reset flags
      std::for_each(_sortedPopulation.begin(), _sortedPopulation.end(),
                    [](auto &i) {
                      auto &id = i.get();
                      id.elite = false;
                      id.extinct = false;
                    });

      // mark elites
      std::for_each(_sortedPopulation.begin(),
                    _sortedPopulation.begin() + _nelites,
                    [](auto &i) { i.get().elite = true; });

      // mark extinct
      std::for_each(_sortedPopulation.end() - _nkills, _sortedPopulation.end(),
                    [](auto &i) { i.get().extinct = true; });

      return Var(_sortedPopulation.front().get().fitness);
    });
  }

private:
  struct MutantInfo {
    MutantInfo(const Mutant &block, std::vector<int> &&indices)
        : block(block), indices(std::move(indices)) {}

    std::reference_wrapper<const Mutant> block;

    OwnedVar state;

    const std::vector<int> indices;
    std::vector<OwnedVar> parameters;
  };

  static inline void gatherMutants(CBChain *chain,
                                   std::vector<MutantInfo> &out);

  struct Individual {
    // Chains are recycled
    CBVar chain{};

    // Keep track of mutants and push/pop mutations on chain
    std::vector<MutantInfo> mutants;

    double fitness{std::numeric_limits<double>::min()};
    bool elite = false;
    bool extinct = false;
  };

  inline void mutate(Individual &individual);
  inline void saveState(Individual &individual);
  inline void restoreState(Individual &individual);
  inline void resetState(Individual &individual);

  tf::Executor &Tasks{Singleton<tf::Executor>::value};
  static inline Parameters _params{
      {"Fitness",
       "The fitness chain, should output a Float fitness value.",
       {CoreInfo::NoneType, CoreInfo::ChainType, CoreInfo::ChainVarType}},
      {"Population", "The population size.", {CoreInfo::IntType}},
      {"Mutation", "The rate of mutation, 0.1 = 10%.", {CoreInfo::FloatType}},
      {"Extinction",
       "The rate of extinction, 0.1 = 10%.",
       {CoreInfo::FloatType}},
      {"Elitism", "The rate of elitism, 0.1 = 10%.", {CoreInfo::FloatType}}};
  ParamVar _baseChain{};
  std::vector<Individual> _population;
  std::vector<std::reference_wrapper<Individual>> _sortedPopulation;
  std::unordered_map<CBChain *, std::reference_wrapper<Individual>>
      _chain2Individual;
  int64_t _popsize = 64;
  double _mutation = 0.2;
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
    case 2:
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
      return _options;
    default:
      return CBVar();
    }
  }

  void cleanup() { _block.cleanup(); }

  void warmup(CBContext *ctx) { _block.warmup(ctx); }

  CBTypeInfo compose(const CBInstanceData &data) {
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

  std::vector<int> mutatingIndices() const {
    if (_indices.valueType == None)
      return {};

    IterableSeq s(_indices);
    std::vector<int> res;
    for (auto &idx : s) {
      res.emplace_back(int(idx.payload.intValue));
    }
    return res;
  }

private:
  friend struct Evolve;
  BlocksVar _block{};
  OwnedVar _options{};
  OwnedVar _indices{};
  static inline Parameters _params{
      {"Block", "The block to mutate.", {CoreInfo::BlockType}},
      {"Indices",
       "The parameter indices to mutate of the inner block.",
       {CoreInfo::IntSeqType}},
      {"Mutations",
       "Mutation table - a table with mutation options.",
       {CoreInfo::NoneType, CoreInfo::AnyTableType}}};
};

void registerBlocks() {
  REGISTER_CBLOCK("Evolve", Evolve);
  REGISTER_CBLOCK("Mutant", Mutant);
}

inline void mutateVar(CBVar &var) {
  switch (var.valueType) {
  case CBType::Int: {
    auto add = Rng::fnormal(0.0, 0.1) * double(var.payload.intValue);
    LOG(DEBUG) << add;
    var.payload.intValue += int64_t(add);
  } break;
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
      auto minfo =
          out.emplace_back(mutator->block, mutator->block.mutatingIndices());
    });
  }
}

inline void Evolve::mutate(Evolve::Individual &individual) {
  std::for_each(std::begin(individual.mutants), std::end(individual.mutants),
                [this](MutantInfo &info) {
                  if (Rng::frand() > _mutation) {
                    LOG(TRACE) << "Skipping a block mutation...";
                    return;
                  }

                  auto options = info.block.get()._options;
                  if (info.block.get().mutant()) {
                    auto mutant = info.block.get().mutant();
                    LOG(TRACE) << "Mutating a block: " << mutant->name(mutant);
                    const auto indices = info.indices.size();
                    if (mutant->mutate && (indices == 0 || rand() < 0.5)) {
                      // In the case the block has `mutate`
                      auto table = options.valueType == Table
                                       ? options.payload.tableValue
                                       : CBTable();
                      mutant->mutate(mutant, table);
                    } else if (indices > 0) {
                      // do stuff on the param
                      // select a random one
                      auto rparam = Rng::rand(indices);
                      auto current =
                          mutant->getParam(mutant, info.indices[rparam]);
                      mutateVar(current);
                      mutant->setParam(mutant, info.indices[rparam], current);
                    }
                  } else {
                    LOG(TRACE) << "No block found to mutate...";
                  }
                });
}

inline void Evolve::saveState(Evolve::Individual &individual) {
  // Store params and state
  std::for_each(std::begin(individual.mutants), std::end(individual.mutants),
                [](MutantInfo &info) {
                  auto mutant = info.block.get().mutant();
                  if (!mutant)
                    return;

                  if (mutant->getState)
                    info.state = mutant->getState(mutant);

                  for (auto idx : info.indices) {
                    info.parameters.push_back(mutant->getParam(mutant, idx));
                  }
                });
}

inline void Evolve::restoreState(Evolve::Individual &individual) {
  // Load params and state
  std::for_each(std::begin(individual.mutants), std::end(individual.mutants),
                [](MutantInfo &info) {
                  auto mutant = info.block.get().mutant();
                  if (!mutant)
                    return;

                  if (mutant->setState && info.state.valueType != None)
                    mutant->setState(mutant, info.state);

                  if (info.parameters.size() == info.indices.size()) {
                    int i = 0;
                    for (auto idx : info.indices) {
                      mutant->setParam(mutant, idx, info.parameters[i]);
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

                  info.state = CBVar();
                  info.parameters.resize(0);
                });
}
} // namespace Genetic
} // namespace chainblocks

#endif /* GENETIC_H */
