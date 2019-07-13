# ChainBlocks
# Inspired by apple Shortcuts actually..
# Simply define a program by chaining blocks (black boxes), that have input and output/s

import sequtils, times, macros, strutils
import fragments/[serialization]
import images

import types
export types

import nimline

when appType != "lib":
  {.compile: "../core/runtime.cpp".}
  when defined windows:
    {.passL: "-lboost_context-mt".}
  else:
    {.passL: "-lboost_context".}
  {.passC: "-DCHAINBLOCKS_RUNTIME".}
else:
  {.emit: """/*INCLUDESECTION*/
#define STB_DS_IMPLEMENTATION 1
""".}

const
  FragCC* = toFourCC("frag")

proc elems*(v: CBInt2): array[2, int64] {.inline.} = [v.toCpp[0].to(int64), v.toCpp[1].to(int64)]
proc elems*(v: CBInt3): array[3, int32] {.inline.} = [v.toCpp[0].to(int32), v.toCpp[1].to(int32), v.toCpp[2].to(int32)]
proc elems*(v: CBInt4): array[4, int32] {.inline.} = [v.toCpp[0].to(int32), v.toCpp[1].to(int32), v.toCpp[2].to(int32), v.toCpp[3].to(int32)]
proc elems*(v: CBFloat2): array[2, float64] {.inline.} = [v.toCpp[0].to(float64), v.toCpp[1].to(float64)]
proc elems*(v: CBFloat3): array[3, float32] {.inline.} = [v.toCpp[0].to(float32), v.toCpp[1].to(float32), v.toCpp[2].to(float32)]
proc elems*(v: CBFloat4): array[4, float32] {.inline.} = [v.toCpp[0].to(float32), v.toCpp[1].to(float32), v.toCpp[2].to(float32), v.toCpp[3].to(float32)]

converter toCBTypesInfo*(s: tuple[types: set[CBType]; canBeSeq: bool]): CBTypesInfo {.inline, noinit.} =
  initSeq(result)
  for t in s.types:
    global.stbds_arrpush(result, CBTypeInfo(basicType: t, sequenced: s.canBeSeq)).to(void)

converter toCBTypesInfo*(s: set[CBType]): CBTypesInfo {.inline, noinit.} = (s, false)

converter toCBTypeInfo*(s: tuple[ty: CBType, isSeq: bool]): CBTypeInfo {.inline, noinit.} =
  CBTypeInfo(basicType: s.ty, sequenced: s.isSeq)

converter toCBTypeInfo*(s: CBType): CBTypeInfo {.inline, noinit.} = (s, false)

converter toCBTypesInfo*(s: seq[CBTypeInfo]): CBTypesInfo {.inline, noinit.} =
  initSeq(result)
  for ti in s:
    result.push ti

converter toCBParameterInfo*(record: tuple[paramName: string; helpText: string; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]; usesContext: bool]): CBParameterInfo {.inline.} =
  # This relies on const strings!! will crash if not!
  result.valueTypes = record.actualTypes.toCBTypesInfo()
  let
    pname = record.paramName
    help = record.helpText
  result.name = pname
  result.help = help
  result.allowContext = record.usesContext

converter toCBParameterInfo*(record: tuple[paramName: string; helpText: string; actualTypes: CBTypesInfo; usesContext: bool]): CBParameterInfo {.inline.} =
  # This relies on const strings!! will crash if not!
  result.valueTypes = record.actualTypes
  let
    pname = record.paramName
    help = record.helpText
  result.name = pname
  result.help = help
  result.allowContext = record.usesContext

converter toCBParameterInfo*(s: tuple[paramName: string; actualTypes: set[CBType]]): CBParameterInfo {.inline, noinit.} = (s.paramName, "", (s.actualTypes, false), false)
converter toCBParameterInfo*(s: tuple[paramName: string; helpText: string; actualTypes: set[CBType]]): CBParameterInfo {.inline, noinit.} = (s.paramName, s.helpText, (s.actualTypes, false), false)
converter toCBParameterInfo*(s: tuple[paramName: string; helpText: string; actualTypes: set[CBType]; usesContext: bool]): CBParameterInfo {.inline, noinit.} = (s.paramName, s.helpText, (s.actualTypes, false), s.usesContext)
converter toCBParameterInfo*(s: tuple[paramName: string; actualTypes: set[CBType], usesContext: bool]): CBParameterInfo {.inline, noinit.} = (s.paramName, "", (s.actualTypes, false), s.usesContext)
converter toCBParameterInfo*(s: tuple[paramName: string; actualTypes: CBTypesInfo, usesContext: bool]): CBParameterInfo {.inline, noinit.} = (s.paramName, "", s.actualTypes, s.usesContext)
converter toCBParameterInfo*(s: tuple[paramName: string; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]]): CBParameterInfo {.inline, noinit.} = (s.paramName, "", (s.actualTypes.types, s.actualTypes.canBeSeq), false)
converter toCBParameterInfo*(s: tuple[paramName: string; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]; usesContext: bool]): CBParameterInfo {.inline, noinit.} = (s.paramName, "", (s.actualTypes.types, s.actualTypes.canBeSeq), s.usesContext)

converter cbParamsToSeq*(params: var CBParametersInfo): seq[CBParameterInfo] =
  for item in params.items:
    result.add item

converter toCBTypesInfoTuple*(s: tuple[a: string; b: set[CBType]]): tuple[c: string; d: CBTypesInfo] {.inline, noinit.} =
  result[0] = s.a
  result[1] = s.b

converter toCBParametersInfo*(s: seq[tuple[paramName: string; helpText: string; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]; usesContext: bool]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: seq[tuple[paramName: string; actualTypes: set[CBType]]]): CBParametersInfo {.inline, noinit.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: seq[tuple[paramName: string; helpText: string; actualTypes: set[CBType]]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: seq[tuple[paramName: string; actualTypes: set[CBType]; usesContext: bool]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: seq[tuple[paramName: string; actualTypes: CBTypesInfo; usesContext: bool]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: seq[tuple[paramName: string; helpText: string; actualTypes: set[CBType]; usesContext: bool]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: seq[tuple[paramName: string; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: seq[tuple[paramName: string; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]; usesContext: bool]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: seq[tuple[paramName: string; actualTypes: seq[CBTypeInfo]]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var
      record = s[i]
      pinfo = CBParameterInfo(name: record.paramName)
    initSeq(pinfo.valueTypes)
    for at in record.actualTypes:
      pinfo.valueTypes.push(at)
    result.push(pinfo)

when isMainModule:
  var
    tys = { Int, Int4, Float3 }
    vtys = tys.toCBTypesInfo()
  assert vtys.len() == 3
  assert vtys[0].basicType == Int
  assert vtys[1].basicType == Int4
  assert vtys[2].basicType == Float3

  var
    tys2 = ({ Int4, Float3 }, true)
    vtys2 = tys2.toCBTypesInfo()
  assert vtys2.len() == 2
  assert vtys2[0].basicType == Int4
  assert vtys2[1].basicType == Float3
  assert vtys2[0].sequenced

  var
    pi1 = ("Param1", { Int, Int4 }).toCBParameterInfo()
  assert pi1.name == "Param1"
  assert pi1.valueTypes[0].basicType == Int
  assert pi1.valueTypes[1].basicType == Int4
  assert not pi1.allowContext

  var
    pi2 = ("Param2", { Int, Int4 }, true).toCBParameterInfo()
  assert pi2.name == "Param2"
  assert pi2.valueTypes[0].basicType == Int
  assert pi2.valueTypes[1].basicType == Int4
  assert pi2.allowContext

  var
    pis1 = @[
      ("Key", { Int }),
      ("Ctrl", { Bool }),
      ("Alt", { Bool }),
      ("Shift", { Bool }),
    ].toCBParametersInfo()
  assert pis1[0].name == "Key"
  assert pis1[0].valueTypes[0].basicType == Int
  assert pis1[1].name == "Ctrl"
  assert pis1[1].valueTypes[0].basicType == Bool

  var
    pis2 = @[("Window", "A window name or variable if we wish to send the event only to a target window.", { String })].toCBParametersInfo()
  assert pis2[0].name == "Window"
  assert pis2[0].help == "A window name or variable if we wish to send the event only to a target window."
  assert pis2[0].valueTypes[0].basicType == String

template iterateTuple(t: tuple; storage: untyped; castType: untyped): untyped =
  var idx = 0
  for x in t.fields:
    storage.toCpp[idx] = x.castType
    inc idx

converter toInt64Value*(v: CBVar): int64 {.inline.} =
  assert v.valueType == Int
  return v.intValue

converter toIntValue*(v: CBVar): int {.inline.} =
  assert v.valueType == Int
  return v.intValue.int

converter toFloatValue*(v: CBVar): float64 {.inline.} =
  assert v.valueType == Float
  return v.floatValue

converter toBoolValue*(v: CBVar): bool {.inline.} =
  assert v.valueType == Bool
  return v.boolValue

converter toCBVar*(cbSeq: CBSeq): CBVar {.inline.} =
  result.valueType = Seq
  result.seqValue = cbSeq

converter toCBVar*(cbStr: CBString): CBVar {.inline.} =
  result.valueType = String
  result.stringValue = cbStr

converter toCString*(cbStr: CBString): cstring {.inline, noinit.} = cast[cstring](cbStr)

converter imageTypesConv*(cbImg: CBImage): Image[uint8] {.inline, noinit.} =
  result.width = cbImg.width
  result.height = cbImg.height
  result.channels = cbImg.channels
  result.data = cbImg.data

converter imageTypesConv*(cbImg: Image[uint8]): CBImage {.inline, noinit.} =
  result.width = cbImg.width.int32
  result.height = cbImg.height.int32
  result.channels = cbImg.channels.int32
  result.data = cbImg.data

proc asCBVar*(v: pointer; vendorId, typeId: FourCC): CBVar {.inline.} =
  return CBVar(valueType: Object, objectValue: v, objectVendorId: vendorId.int32, objectTypeId: typeId.int32)

converter toCBVar*(v: int): CBVar {.inline.} =
  return CBVar(valueType: Int, intValue: v)

converter toCBVar*(v: int64): CBVar {.inline.} =
  return CBVar(valueType: Int, intValue: v)

converter toCBVar*(v: CBInt2): CBVar {.inline.} =
  return CBVar(valueType: Int2, int2Value: v)

converter toCBVar*(v: CBInt3): CBVar {.inline.} =
  return CBVar(valueType: Int3, int3Value: v)

converter toCBVar*(v: CBInt4): CBVar {.inline.} =
  return CBVar(valueType: Int4, int4Value: v)

converter toCBVar*(v: tuple[a,b: int]): CBVar {.inline.} =
  result.valueType = Int2
  iterateTuple(v, result.int2Value, int64)

converter toCBVar*(v: tuple[a,b: int64]): CBVar {.inline.} =
  result.valueType = Int2
  iterateTuple(v, result.int2Value, int64)

converter toCBVar*(v: tuple[a,b,c: int]): CBVar {.inline.} =
  result.valueType = Int3
  iterateTuple(v, result.int3Value, int32)

converter toCBVar*(v: tuple[a,b,c: int32]): CBVar {.inline.} =
  result.valueType = Int3
  iterateTuple(v, result.int3Value, int32)

converter toCBVar*(v: tuple[a,b,c,d: int]): CBVar {.inline.} =
  result.valueType = Int4
  iterateTuple(v, result.int4Value, int32)

converter toCBVar*(v: tuple[a,b,c,d: int32]): CBVar {.inline.} =
  result.valueType = Int4
  iterateTuple(v, result.int4Value, int32)

converter toCBVar*(v: bool): CBVar {.inline.} =
  return CBVar(valueType: Bool, boolValue: v)

converter toCBVar*(v: float32): CBVar {.inline.} =
  return CBVar(valueType: Float, floatValue: v)

converter toCBVar*(v: float64): CBVar {.inline.} =
  return CBVar(valueType: Float, floatValue: v)

converter toCBVar*(v: CBFloat2): CBVar {.inline.} =
  return CBVar(valueType: Float2, float2Value: v)

converter toCBVar*(v: CBFloat3): CBVar {.inline.} =
  return CBVar(valueType: Float3, float3Value: v)

converter toCBVar*(v: CBFloat4): CBVar {.inline.} =
  return CBVar(valueType: Float4, float4Value: v)

converter toCBVar*(v: tuple[a,b: float64]): CBVar {.inline.} =
  result.valueType = Float2
  iterateTuple(v, result.float2Value, float64)

converter toCBVar*(v: tuple[a,b,c: float]): CBVar {.inline.} =
  result.valueType = Float3
  iterateTuple(v, result.float3Value, float32)

converter toCBVar*(v: tuple[a,b,c: float32]): CBVar {.inline.} =
  result.valueType = Float3
  iterateTuple(v, result.float3Value, float32)

converter toCBVar*(v: tuple[a,b,c,d: float]): CBVar {.inline.} =
  result.valueType = Float4
  iterateTuple(v, result.float4Value, float32)

converter toCBVar*(v: tuple[a,b,c,d: float32]): CBVar {.inline.} =
  result.valueType = Float4
  iterateTuple(v, result.float4Value, float32)

converter toCBVar*(v: tuple[r,g,b,a: uint8]): CBVar {.inline.} =
  return CBVar(valueType: Color, colorValue: CBColor(r: v.r, g: v.g, b: v.b, a: v.a))

converter toCBVar*(v: CBColor): CBVar {.inline.} =
  return CBVar(valueType: Color, colorValue: v)

converter toCBVar*(v: ptr CBChainPtr): CBVar {.inline.} =
  return CBVar(valueType: Chain, chainValue: v)

converter toCBVar*(v: var CBChainPtr): CBVar {.inline.} =
  return CBVar(valueType: Chain, chainValue: addr v)

template contextOrPure*(subject, container: untyped; wantedType: CBType; typeGetter: untyped): untyped =
  if container.valueType == ContextVar:
    var foundVar = context.contextVariable(container.stringValue)
    if foundVar[].valueType == wantedType:
      subject = foundVar[].typeGetter
  else:
    subject = container.typeGetter

include ops

proc contextVariable*(name: string): CBVar {.inline.} =
  return CBVar(valueType: ContextVar, stringValue: name)

template `~~`*(name: string): CBVar = contextVariable(name)

template withChain*(chain, body: untyped): untyped =
  var `chain` {.inject.} = newChain(astToStr(`chain`))
  var prev = getCurrentChain()
  setCurrentChain(chain)
  body
  setCurrentChain(prev)

# Make those optional
template help*(b: auto): cstring = ""
template setup*(b: auto) = discard
template destroy*(b: auto) = discard
template preChain*(b: auto; context: CBContext) = discard
template postChain*(b: auto; context: CBContext) = discard
template exposedVariables*(b: auto): CBParametersInfo = @[]
template consumedVariables*(b: auto): CBParametersInfo = @[]
template parameters*(b: auto): CBParametersInfo = @[]
template setParam*(b: auto; index: int; val: CBVar) = discard
template getParam*(b: auto; index: int): CBVar = CBVar(valueType: None)
template cleanup*(b: auto) = discard

when appType != "lib":
  proc createBlock*(name: cstring): ptr CBRuntimeBlock {.importcpp: "chainblocks::createBlock(#)", header: "runtime.hpp".}

  proc getCurrentChain*(): CBChainPtr {.importcpp: "chainblocks::getCurrentChain()", header: "runtime.hpp".}
  proc setCurrentChain*(chain: CBChainPtr) {.importcpp: "chainblocks::setCurrentChain(#)", header: "runtime.hpp".}
  proc registerChain*(chain: CBChainPtr) {.importcpp: "chainblocks::registerChain(#)", header: "runtime.hpp".}
  proc unregisterChain*(chain: CBChainPtr) {.importcpp: "chainblocks::unregisterChain(#)", header: "runtime.hpp".}
  proc add*(chain: CBChainPtr; blk: ptr CBRuntimeBlock) {.importcpp: "#.addBlock(#)", header: "runtime.hpp".}

  proc globalVariable*(name: cstring): ptr CBVar {.importcpp: "chainblocks::globalVariable(#)", header: "runtime.hpp".}
  proc hasGlobalVariable*(name: cstring): bool {.importcpp: "chainblocks::hasGlobalVariable(#)", header: "runtime.hpp".}

  proc startInternal(chain: CBChainPtr; loop: bool = false) {.importcpp: "chainblocks::start(#, #)", header: "runtime.hpp".}
  proc start*(chain: CBChainPtr; loop: bool = false) {.inline.} =
    var frame = getFrameState()
    startInternal(chain, loop)
    setFrameState(frame)
  
  proc tickInternal(chain: CBChainPtr; rootInput: CBVar = Empty) {.importcpp: "chainblocks::tick(#, #)", header: "runtime.hpp".}
  proc tick*(chain: CBChainPtr; rootInput: CBVar = Empty) {.inline.} =
    var frame = getFrameState()
    tickInternal(chain, rootInput)
    setFrameState(frame)
  
  proc stopInternal(chain: CBChainPtr): CBVar {.importcpp: "chainblocks::stop(#)", header: "runtime.hpp".}
  proc stop*(chain: CBChainPtr): CBVar {.inline.} =
    var frame = getFrameState()
    result = stopInternal(chain)
    setFrameState(frame)

  proc store*(chain: CBChainPtr): string =
    let str = invokeFunction("chainblocks::store", chain).to(StdString)
    $(str.c_str().to(cstring))
  
  proc store*(v: CBVar): string =
    let str = invokeFunction("chainblocks::store", v).to(StdString)
    $(str.c_str().to(cstring))
  
  proc load*(chain: var CBChainPtr; jsonString: cstring) {.importcpp: "chainblocks::load(#, #)", header: "runtime.hpp".}
  proc load*(chain: var CBVar; jsonString: cstring) {.importcpp: "chainblocks::load(#, #)", header: "runtime.hpp".}

  var
    compileTimeMode {.compileTime.}: bool = false
    staticChainBlocks {.compileTime.}: seq[tuple[blk: NimNode; params: seq[NimNode]]]

  proc generateInitMultiple*(rtName: string; ctName: typedesc; args: seq[tuple[label, value: NimNode]]): NimNode {.compileTime.} =
    if not compileTimeMode:
      # Generate vars
      # Setup
      # Validate params
      # Fill params
      # Add to chain
      var
        sym = gensym(nskVar)
        paramNames = gensym(nskVar)
        paramSets = gensym(nskVar)
      result = quote do:
        var
          `sym` = createBlock(`rtName`)
        `sym`.setup(`sym`)
        var
          cparams = `sym`.parameters(`sym`)
          paramsSeq = cparams.cbParamsToSeq()
          `paramNames` = paramsSeq.map do (x: CBParameterInfo) -> string: ($x.name).toLowerAscii
          `paramSets` = paramsSeq.map do (x: CBParameterInfo) -> CBTypesInfo: x.valueTypes
      
      for i in 0..`args`.high:
        var
          val = `args`[i].value
          lab = newStrLitNode($`args`[i].label.strVal)
        result.add quote do:
          var
            cbVar: CBVar = `val`
            label = `lab`.toLowerAscii
            idx = `paramNames`.find(label)
          assert idx != -1, "Could not find parameter: " & label
          # TODO
          # assert cbVar.valueType in `paramSets`[idx]
          `sym`.setParam(`sym`, idx, cbVar)
      
      result.add quote do:
        getCurrentChain().add(`sym`)
    else:
      var
        sym = gensym(nskVar)
        params: seq[NimNode]
      for arg in args:
        params.add arg.value
      result = quote do:
        var `sym`: `ctName`
      staticChainBlocks.add((sym, params))
      
  proc generateInitSingle*(rtName: string; ctName: typedesc; args: NimNode): NimNode {.compileTime.} =
    if not compileTimeMode:
      result = quote do:
        var
          b = createBlock(`rtName`)
        b.setup(b)
        
        var cbVar: CBVar = `args`
        b.setParam(b, 0, cbVar)
        getCurrentChain().add(b)
    else:
      var
        sym = gensym(nskVar)
        params = @[args]
      result = quote do:
        var `sym`: `ctName`
      staticChainBlocks.add((sym, params))

  proc generateInit*(rtName: string; ctName: typedesc): NimNode {.compileTime.} =
    if not compileTimeMode:
      result = quote do:
        var
          b = createBlock(`rtName`)
        b.setup(b)
        getCurrentChain().add(b)
    else:
      var
        sym = gensym(nskVar)
        params: seq[NimNode] = @[]
      result = quote do:
        var `sym`: `ctName`
      staticChainBlocks.add((sym, params))

when appType == "lib" and not defined(nimV2):
  template updateStackBottom*() =
    var sp {.volatile.}: pointer
    sp = addr(sp)
    nimGC_setStackBottom(sp)
    when compileOption("threads"):
      setupForeignThreadGC()
else:
  template updateStackBottom*() =
    discard

macro chainblock*(blk: untyped; blockName: string; namespaceStr: string = ""; testCode: untyped = nil): untyped =
  var
    rtName = ident($blk & "RT")
    rtNameValue = ident($blk & "RTValue")
    macroName {.used.} = ident($blockName)
    namespace = if $namespaceStr != "": $namespaceStr & "." else: ""
    testName {.used.} = ident("test_" & $blk)

    nameProc = ident($blk & "_name")
    helpProc = ident($blk & "_help")

    setupProc = ident($blk & "_setup")
    destroyProc = ident($blk & "_destroy")
    
    preChainProc = ident($blk & "_preChain")
    postChainProc = ident($blk & "_postChain")
    
    inputTypesProc = ident($blk & "_inputTypes")
    outputTypesProc = ident($blk & "_outputTypes")
    
    exposedVariablesProc = ident($blk & "_exposedVariables")
    consumedVariablesProc = ident($blk & "_consumedVariables")
    
    parametersProc = ident($blk & "_parameters")
    setParamProc = ident($blk & "_setParam")
    getParamProc = ident($blk & "_getParam")
    
    activateProc = ident($blk & "_activate")
    cleanupProc = ident($blk & "_cleanup")
  
  result = quote do:
    import macros, strutils, sequtils

    type
      `rtNameValue` = object
        pre: CBRuntimeBlock
        sb: `blk`
        cacheInputTypes: owned(ref CBTypesInfo)
        cacheOutputTypes: owned(ref CBTypesInfo)
        cacheExposedVariables: owned(ref CBParametersInfo)
        cacheConsumedVariables: owned(ref CBParametersInfo)
        cacheParameters: owned(ref CBParametersInfo)
      
      `rtName`* = ptr `rtNameValue`
    
    template name*(b: `blk`): string =
      (`namespace` & `blockName`)
    proc `nameProc`*(b: `rtName`): cstring {.cdecl.} =
      updateStackBottom()
      (`namespace` & `blockName`)
    proc `helpProc`*(b: `rtName`): cstring {.cdecl.} =
      updateStackBottom()
      b.sb.help()
    proc `setupProc`*(b: `rtName`) {.cdecl.} =
      updateStackBottom()
      b.sb.setup()
    proc `destroyProc`*(b: `rtName`) {.cdecl.} =
      updateStackBottom()
      b.sb.destroy()
      # manually destroy nim side
      when defined nimV2:
        if b.cacheInputTypes != nil:
          freeSeq(b.cacheInputTypes[])
          dispose(b.cacheInputTypes)
        if b.cacheOutputTypes != nil:
          freeSeq(b.cacheOutputTypes[])
          dispose(b.cacheOutputTypes)
        if b.cacheExposedVariables != nil:
          for item in b.cacheExposedVariables[].mitems:
            freeSeq(item.valueTypes)
          freeSeq(b.cacheExposedVariables[])
          dispose(b.cacheExposedVariables)
        if b.cacheConsumedVariables != nil:
          for item in b.cacheConsumedVariables[].mitems:
            freeSeq(item.valueTypes)
          freeSeq(b.cacheConsumedVariables[])
          dispose(b.cacheConsumedVariables)
        if b.cacheParameters != nil:
          for item in b.cacheParameters[].mitems:
            freeSeq(item.valueTypes)
          freeSeq(b.cacheParameters[])
          dispose(b.cacheParameters)
      delCPP(b)
    proc `preChainProc`*(b: `rtName`, context: CBContext) {.cdecl.} =
      updateStackBottom()
      b.sb.preChain(context)
    proc `postChainProc`*(b: `rtName`, context: CBContext) {.cdecl.} =
      updateStackBottom()
      b.sb.postChain(context)
    proc `inputTypesProc`*(b: `rtName`): CBTypesInfo {.cdecl.} =
      updateStackBottom()
      if b.cacheInputTypes == nil:
        new b.cacheInputTypes
        b.cacheInputTypes[] = b.sb.inputTypes()
      b.cacheInputTypes[]
    proc `outputTypesProc`*(b: `rtName`): CBTypesInfo {.cdecl.} =
      updateStackBottom()
      if b.cacheOutputTypes == nil:
        new b.cacheOutputTypes
        b.cacheOutputTypes[] = b.sb.outputTypes()
      b.cacheOutputTypes[]
    proc `exposedVariablesProc`*(b: `rtName`): CBParametersInfo {.cdecl.} =
      updateStackBottom()
      if b.cacheExposedVariables == nil:
        new b.cacheExposedVariables
        b.cacheExposedVariables[] = b.sb.exposedVariables()
      b.cacheExposedVariables[]
    proc `consumedVariablesProc`*(b: `rtName`): CBParametersInfo {.cdecl.} =
      updateStackBottom()
      if b.cacheConsumedVariables == nil:
        new b.cacheConsumedVariables
        b.cacheConsumedVariables[] = b.sb.consumedVariables()
      b.cacheConsumedVariables[]
    proc `parametersProc`*(b: `rtName`): CBParametersInfo {.cdecl.} =
      updateStackBottom()
      if b.cacheParameters == nil:
        new b.cacheParameters
        b.cacheParameters[] = b.sb.parameters()
      b.cacheParameters[]
    proc `setParamProc`*(b: `rtName`; index: int; val: CBVar) {.cdecl.} =
      updateStackBottom()
      b.sb.setParam(index, val)
    proc `getParamProc`*(b: `rtName`; index: int): CBVar {.cdecl.} =
      updateStackBottom()
      b.sb.getParam(index)
    proc `activateProc`*(b: `rtName`; context: CBContext; input: CBVar): CBVar {.cdecl.} =
      updateStackBottom()
      b.sb.activate(context, input)
    proc `cleanupProc`*(b: `rtName`) {.cdecl.} =
      updateStackBottom()
      b.sb.cleanup()
    registerBlock(`namespace` & `blockName`) do -> ptr CBRuntimeBlock {.cdecl.}:
      newCBRuntimeBlock(result, `rtNameValue`)
      # DO NOT CHANGE THE FOLLOWING, this sorcery is needed to build with msvc 19ish
      # Moreover it's kinda nim's fault, as it won't generate a C cast without `.pointer`
      result.name = cast[CBNameProc](`nameProc`.pointer)
      result.help = cast[CBHelpProc](`helpProc`.pointer)
      result.setup = cast[CBSetupProc](`setupProc`.pointer)
      result.destroy = cast[CBDestroyProc](`destroyProc`.pointer)
      result.preChain = cast[CBPreChainProc](`preChainProc`.pointer)
      result.postChain = cast[CBPostChainProc](`postChainProc`.pointer)
      result.inputTypes = cast[CBInputTypesProc](`inputTypesProc`.pointer)
      result.outputTypes = cast[CBOutputTypesProc](`outputTypesProc`.pointer)
      result.exposedVariables = cast[CBExposedVariablesProc](`exposedVariablesProc`.pointer)
      result.consumedVariables = cast[CBConsumedVariablesProc](`consumedVariablesProc`.pointer)
      result.parameters = cast[CBParametersProc](`parametersProc`.pointer)
      result.setParam = cast[CBSetParamProc](`setParamProc`.pointer)
      result.getParam = cast[CBGetParamProc](`getParamProc`.pointer)
      result.activate = cast[CBActivateProc](`activateProc`.pointer)
      result.cleanup = cast[CBCleanupProc](`cleanupProc`.pointer)

    static:
      echo "Registered chainblock: " & `namespace` & `blockName`
  
  when appType != "lib":
    if $namespaceStr != "":
      var nameSpaceType = ident($namespaceStr)
      # DSL Utilities
      result.add quote do:
        macro `macroName`*(_: typedesc[`nameSpaceType`]): untyped =
          result = generateInit(`namespace` & `blockName`, `blk`)

        macro `macroName`*(_: typedesc[`nameSpaceType`]; args: untyped): untyped =
          if args.kind == nnkStmtList:
            # We need to transform the stmt list
            var argNodes = newSeq[tuple[label, value: NimNode]]()
            for arg in args:
              assert arg.kind == nnkCall, "Syntax error" # TODO better errors
              var
                labelNode = arg[0]
                varNode = arg[1][0]
              argNodes.add (labelNode, varNode)
            result = generateInitMultiple(`namespace` & `blockName`, `blk`, argNodes)
          else:
            # assume single? maybe need to be precise to filter mistakes
            result = generateInitSingle(`namespace` & `blockName`, `blk`, args)
    else:
      result.add quote do:
        # DSL Utilities
        macro `macroName`*(): untyped =
          result = generateInit(`blockName`, `blk`)

        macro `macroName`*(args: untyped): untyped =
          if args.kind == nnkStmtList:
            # We need to transform the stmt list
            var argNodes = newSeq[tuple[label, value: NimNode]]()
            for arg in args:
              assert arg.kind == nnkCall, "Syntax error" # TODO better errors
              var
                labelNode = arg[0]
                varNode = arg[1][0]
              argNodes.add (labelNode, varNode)
            result = generateInitMultiple(`blockName`, `blk`, argNodes)
          else:
            # assume single? maybe need to be precise to filter mistakes
            result = generateInitSingle(`blockName`, `blk`, args)
      
    if testCode != nil:
      result.add quote do:
        when defined blocksTesting:
          proc `testName`() =
            `testCode`
          `testName`()

when appType != "lib":
  # When building the runtime!

  proc registerBlock*(name: string; initProc: CBBlockConstructor) =
    invokeFunction("chainblocks::registerBlock", name, initProc).to(void)

  proc registerObjectType*(vendorId, typeId: FourCC; info: CBObjectInfo) =
    invokeFunction("chainblocks::registerObjectType", vendorId, typeId, info).to(void)

  proc registerEnumType*(vendorId, typeId: FourCC; info: CBEnumInfo) =
    invokeFunction("chainblocks::registerEnumType", vendorId, typeId, info).to(void)

  proc registerRunLoopCallback*(name: string; callback: CBOnRunLoopTick) =
    invokeFunction("chainblocks::registerRunLoopCallback", name, callback).to(void)

  proc unregisterRunLoopCallback*(name: string) =
    invokeFunction("chainblocks::unregisterRunLoopCallback", name).to(void)

  proc contextVariable*(ctx: CBContext; name: cstring): ptr CBVar {.importcpp: "chainblocks::contextVariable(#, #)", header: "chainblocks.hpp".}

  proc setError*(ctx: CBContext; errorTxt: cstring) {.importcpp: "#->setError(#)", header: "chainblocks.hpp".}

  proc newChain*(name: string): CBChainPtr {.inline.} =
    newCBChain(result, CBChain, name.cstring)
    registerChain(result)
  
  proc destroy*(chain: CBChainPtr) {.inline.} =
    unregisterChain(chain)
    delCPP(chain)
  
  proc runChain*(chain: CBChainPtr, context: ptr CBContextObj; chainInput: CBVar): StdTuple2[bool, CBVar] {.importcpp: "chainblocks::runChain(#, #, #)", header: "chainblocks.hpp".}
  proc suspendInternal(seconds: float64): CBVar {.importcpp: "chainblocks::suspend(#)", header: "chainblocks.hpp".}
  proc suspend*(seconds: float64): CBVar {.inline.} =
    var frame = getFrameState()
    result = suspendInternal(seconds)
    setFrameState(frame)

  proc sleep*(secs: float64) {.importcpp: "chainblocks::sleep(#)", header: "chainblocks.hpp".}
  
else:
  # When we are a dll with a collection of blocks!
  
  import dynlib
  
  type
    RegisterBlkProc = proc(name: cstring; initProc: CBBlockConstructor) {.cdecl.}
    RegisterTypeProc = proc(vendorId, typeId: FourCC; info: CBObjectInfo) {.cdecl.}
    RegisterLoopCb = proc(name: cstring; cb: CBOnRunLoopTick) {.cdecl.}
    UnregisterLoopCb = proc(name: cstring) {.cdecl.}
    CtxVariableProc = proc(ctx: CBContext; name: cstring): ptr CBVar {.cdecl.}
    CtxSetErrorProc = proc(ctx: CBContext; errorTxt: cstring) {.cdecl.}
    SuspendProc = proc(seconds: float64): CBVar {.cdecl.}
  
  var
    exeLib = loadLib()
    cbRegisterBlock = cast[RegisterBlkProc](exeLib.symAddr("chainblocks_RegisterBlock"))
    cbRegisterObjectType = cast[RegisterTypeProc](exeLib.symAddr("chainblocks_RegisterObjectType"))
    cbRegisterLoopCb = cast[RegisterLoopCb](exeLib.symAddr("chainblocks_RegisterRunLoopCallback"))
    cbUnregisterLoopCb = cast[UnregisterLoopCb](exeLib.symAddr("chainblocks_UnregisterRunLoopCallback"))
    cbContextVar = cast[CtxVariableProc](exeLib.symAddr("chainblocks_ContextVariable"))
    cbSetError = cast[CtxSetErrorProc](exeLib.symAddr("chainblocks_SetError"))
    cbSuspend = cast[SuspendProc](exeLib.symAddr("chainblocks_Suspend"))

  proc registerBlock*(name: string; initProc: CBBlockConstructor) = cbRegisterBlock(name, initProc)
  proc registerObjectType*(vendorId, typeId: FourCC; info: CBObjectInfo) = cbRegisterObjectType(vendorId, typeId, info)
  proc registerRunLoopCallback*(name: string; callback: CBOnRunLoopTick) = cbRegisterLoopCb(name, callback)
  proc unregisterRunLoopCallback*(name: string) = cbUnregisterLoopCb(name)

  proc contextVariable*(ctx: CBContext; name: cstring): ptr CBVar {.inline.} = cbContextVar(ctx, name)
  proc setError*(ctx: CBContext; errorTxt: cstring) {.inline.} = cbSetError(ctx, errorTxt)
  
  proc suspend*(seconds: float64): CBVar {.inline.} =
    var frame = getFrameState()
    result = cbSuspend(seconds)
    setFrameState(frame)

# This template is inteded to be used inside blocks activations
template pause*(secs: float): untyped =
  let suspendRes = suspend(secs.float64)
  if suspendRes.chainState != Continue:
    return suspendRes

template halt*(context: CBContext; txt: string): untyped =
  context.setError(txt)
  StopChain

defineCppType(StdRegex, "std::regex", "<regex>")
defineCppType(StdSmatch, "std::smatch", "<regex>")
defineCppType(StdSSubMatch, "std::ssub_match", "<regex>")

when not defined(skipCoreBlocks):
  import unicode
  include blocks/internal/[core, strings, stack, calculate]

when appType != "lib":
  # Swaps from compile time chain mode on/off
  macro setCompileTimeChain*(enabled: bool) = compileTimeMode = enabled.boolVal

  template compileTimeChain*(body: untyped): untyped =
    setCompileTimeChain(true)
    body
    setCompileTimeChain(false)

  # Unrolls the setup and params settings of the current compile time chain once
  macro prepareCompileTimeChain*(): untyped =
    result = newStmtList()

    for blkData in staticChainBlocks:
      var
        blk = blkData.blk
        params = blkData.params
      result.add quote do:
        `blk`.setup()
      for i in 0..params.high:
        var param = params[i]
        result.add quote do:
          `blk`.setParam(`i`, `param`)

  # Unrolls the current compile time chain once
  macro synthesizeCompileTimeChain*(input: untyped): untyped =
    result = newStmtList()
    var
      currentOutput = input
      ctx = gensym(nskVar)

    result.add quote do:
      var `ctx`: CBContext

    for blkData in staticChainBlocks:
      var blk = blkData.blk
      result.add quote do:
        `currentOutput` = `blk`.activate(`ctx`, `currentOutput`)
    
    result.add quote do:
      `currentOutput`
    
  # Clear the compile time chain state
  macro clearCompileTimeChain*() = staticChainBlocks.setLen(0) # consume all

# always try this for safety
assert sizeof(CBVar) == 48

when isMainModule:
  import os
  
  type
    CBPow2Block = object
      inputValue: float
      params: array[1, CBVar]
  
  template inputTypes*(b: CBPow2Block): CBTypesInfo = { Float }
  template outputTypes*(b: CBPow2Block): CBTypesInfo = { Float }
  template parameters*(b: CBPow2Block): CBParametersInfo = @[("test", { Int })]
  template setParam*(b: CBPow2Block; index: int; val: CBVar) = b.params[0] = val
  template getParam*(b: CBPow2Block; index: int): CBVar = b.params[0]
  template activate*(b: CBPow2Block; context: CBContext; input: CBVar): CBVar = (input.floatValue * input.floatValue).CBVar

  chainblock CBPow2Block, "Pow2StaticBlock"

  proc run() =
    var
      pblock1: CBPow2Block
    pblock1.setup()
    var res1 = pblock1.activate(nil, 2.0)
    echo res1.float
    
    var pblock2 = createBlock("Pow2StaticBlock")
    assert pblock2 != nil
    var param0Var = 10.CBVar
    echo param0Var.valueType
    pblock2.setParam(pblock2, 0, param0Var)
    echo pblock2.getParam(pblock2, 0).valueType
    var res2 = pblock2.activate(pblock2, nil, 2.0)
    echo res2.float

    withChain inlineTesting:
      Const 10
      Math.Add 10
      Log()
    inlineTesting.start()
    assert inlineTesting.stop() == 20.CBVar
    
    var f4seq: CBSeq
    initSeq(f4seq)
    f4seq.push (2.0, 3.0, 4.0, 5.0).CBVar
    f4seq.push (6.0, 7.0, 8.0, 9.0).CBVar
    withChain inlineTesting2:
      Const f4seq
      Math.Add (0.1, 0.2, 0.3, 0.4)
      Log()
    inlineTesting2.start()
    
    var
      mainChain = newChain("mainChain")
    setCurrentChain(mainChain)

    withChain chain1:
      Msg "SubChain!"
      Sleep 0.2

    chain1.start(true)
    for i in 0..3:
      echo "Iteration ", i
      chain1.tick()
      sleep(500)
    discard chain1.stop()
    echo "Stopped"
    chain1.tick() # should do nothing
    echo "Done"

    var
      subChain1 = newChain("subChain1")
    setCurrentChain(subChain1)
    
    Const "Hey hey"
    Log()
    
    setCurrentChain(mainChain)
    
    Log()
    Msg "Hello"
    Msg "World"
    Const 15
    If:
      Operator: CBVar(valueType: Enum, enumValue: MoreEqual.CBEnum, enumVendorId: FragCC.int32, enumTypeId: BoolOpCC.int32)
      Operand: 10
      True: subChain1
    Sleep 0.0
    Const 11
    ToString()
    Log()
    
    mainChain.start(true)
    for i in 0..10:
      var idx = i.CBVar
      mainChain.tick(idx)
    
    discard mainChain.stop()

    mainChain.start(true)
    for i in 0..10:
      var idx = i.CBVar
      mainChain.tick(idx)

    let
      jstr = mainChain.store()
    assert jstr == """{"blocks":[{"name":"Log","params":[]},{"name":"Msg","params":[{"name":"Message","value":{"type":13,"value":"Hello"}}]},{"name":"Msg","params":[{"name":"Message","value":{"type":13,"value":"World"}}]},{"name":"Const","params":[{"name":"Value","value":{"type":5,"value":15}}]},{"name":"If","params":[{"name":"Operator","value":{"type":3,"typeId":1819242338,"value":3,"vendorId":1734439526}},{"name":"Operand","value":{"type":5,"value":10}},{"name":"True","value":{"name":"subChain1","type":18}},{"name":"False","value":{"type":0,"value":0}}]},{"name":"Sleep","params":[{"name":"Time","value":{"type":9,"value":0}}]},{"name":"Const","params":[{"name":"Value","value":{"type":5,"value":11}}]},{"name":"ToString","params":[]},{"name":"Log","params":[]}],"name":"mainChain","version":0.1}"""
    echo jstr
    var jchain: CBChainPtr
    load(jchain, jstr)
    let
      jstr2 = jchain.store()
    assert jstr2 == """{"blocks":[{"name":"Log","params":[]},{"name":"Msg","params":[{"name":"Message","value":{"type":13,"value":"Hello"}}]},{"name":"Msg","params":[{"name":"Message","value":{"type":13,"value":"World"}}]},{"name":"Const","params":[{"name":"Value","value":{"type":5,"value":15}}]},{"name":"If","params":[{"name":"Operator","value":{"type":3,"typeId":1819242338,"value":3,"vendorId":1734439526}},{"name":"Operand","value":{"type":5,"value":10}},{"name":"True","value":{"name":"subChain1","type":18}},{"name":"False","value":{"type":0,"value":0}}]},{"name":"Sleep","params":[{"name":"Time","value":{"type":9,"value":0}}]},{"name":"Const","params":[{"name":"Value","value":{"type":5,"value":11}}]},{"name":"ToString","params":[]},{"name":"Log","params":[]}],"name":"mainChain","version":0.1}"""

    echo sizeof(CBVar)

    compileTimeChain:
      Msg "Hello"
      Msg "Static"
      Msg "World"
    
    prepareCompileTimeChain()
    discard synthesizeCompileTimeChain(Empty)
    clearCompileTimeChain()
  
  run()
  
  echo "Done"