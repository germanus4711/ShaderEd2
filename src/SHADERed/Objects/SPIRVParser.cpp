#include <SHADERed/Objects/SPIRVParser.h>
#include <spvgentwo/Spv.h>
#include <unordered_map>
#include <functional>

typedef unsigned int spv_word;

namespace ed {
	std::string spvReadString(const unsigned int* data, int length, int& i)
	{
		std::string ret(length * 4, 0);

		for (int j = 0; j < length; j++, i++) {
			ret[j*4+0] = (data[i] & 0x000000FF);
			ret[j*4+1] = (data[i] & 0x0000FF00) >> 8;
			ret[j*4+2] = (data[i] & 0x00FF0000) >> 16;
			ret[j*4+3] = (data[i] & 0xFF000000) >> 24;
		}

		for (size_t j = 0; j < ret.size(); j++) {
			if (ret[j] == 0) {
				ret.resize(j);
				break;
			}
		}

		return ret;
	}

	void SPIRVParser::Parse(const std::vector<unsigned int>& ir, bool trimFunctionNames)
	{
		Functions.clear();
		UserTypes.clear();
		Uniforms.clear();
		Globals.clear();

		ArithmeticInstCount = 0;
		BitInstCount = 0;
		LogicalInstCount = 0;
		TextureInstCount = 0;
		DerivativeInstCount = 0;
		ControlFlowInstCount = 0;

		BarrierUsed = false;
		LocalSizeX = 1;
		LocalSizeY = 1;
		LocalSizeZ = 1;

		std::string curFunc = "";
		int lastOpLine = -1;

		std::unordered_map<spv_word, std::string> names;
		std::unordered_map<spv_word, spv_word> pointers;
		std::unordered_map<spv_word, std::pair<ValueType, int>> types;

		std::function<void(Variable&, spv_word)> fetchType = [&](Variable& var, spv_word type) {
			spv_word actualType = type;
			if (pointers.count(type))
				actualType = pointers[type];

			const std::pair<ValueType, int>& info = types[actualType];
			var.Type = info.first;
			
			if (var.Type == ValueType::Struct)
				var.TypeName = names[info.second];
			else if (var.Type == ValueType::Vector || var.Type == ValueType::Matrix) {
				var.TypeComponentCount = info.second & 0x00ffffff;
				var.BaseType = (ValueType)((info.second & 0xff000000) >> 24);
			} 
		};

		for (int i = 5; i < ir.size();) {
			int iStart = i;
			spv_word opcodeData = ir[i];

			spv_word wordCount = ((opcodeData & (~spvgentwo::spv::OpCodeMask)) >> spvgentwo::spv::WordCountShift) - 1;
			spvgentwo::spv::Op opcode = (spvgentwo::spv::Op)(opcodeData & spvgentwo::spv::OpCodeMask);

			switch (opcode) {
			case spvgentwo::spv::Op::OpName: {
				spv_word loc = ir[++i];
				spv_word stringLength = wordCount - 1;

				names[loc] = spvReadString(ir.data(), stringLength, ++i);
			} break;
			case spvgentwo::spv::Op::OpLine: {
				++i; // skip file
				lastOpLine = ir[++i];

				if (!curFunc.empty() && Functions[curFunc].LineStart == -1)
					Functions[curFunc].LineStart = lastOpLine;
			} break;
			case spvgentwo::spv::Op::OpTypeStruct: {
				spv_word loc = ir[++i];

				spv_word memCount = wordCount - 1;
				if (UserTypes.count(names[loc]) == 0) {
					std::vector<Variable> mems(memCount);
					for (spv_word j = 0; j < memCount; j++) {
						spv_word type = ir[++i];
						fetchType(mems[j], type);
					}

					UserTypes.insert(std::make_pair(names[loc], mems));
				} else {
					auto& typeInfo = UserTypes[names[loc]];
					for (spv_word j = 0; j < memCount && j < typeInfo.size(); j++) {
						spv_word type = ir[++i];
						fetchType(typeInfo[j], type);
					}
				}

				types[loc] = std::make_pair(ValueType::Struct, loc);
			} break;
			case spvgentwo::spv::Op::OpMemberName: {
				spv_word owner = ir[++i];
				spv_word index = ir[++i]; // index

				spv_word stringLength = wordCount - 2;

				auto& typeInfo = UserTypes[names[owner]];

				if (index < typeInfo.size())
					typeInfo[index].Name = spvReadString(ir.data(), stringLength, ++i);
				else {
					typeInfo.resize(index + 1);
					typeInfo[index].Name = spvReadString(ir.data(), stringLength, ++i);
				}
			} break;
			case spvgentwo::spv::Op::OpFunction: {
				spv_word type = ir[++i];
				spv_word loc = ir[++i];

				curFunc = names[loc];

				if (trimFunctionNames) {
					size_t args = curFunc.find_first_of('(');
					if (args != std::string::npos)
						curFunc = curFunc.substr(0, args);
				}

				fetchType(Functions[curFunc].ReturnType, type);
				Functions[curFunc].LineStart = -1;
			} break;
			case spvgentwo::spv::Op::OpFunctionEnd: {
				Functions[curFunc].LineEnd = lastOpLine;
				lastOpLine = -1;
				curFunc = "";
			} break;
			case spvgentwo::spv::Op::OpVariable: {
				spv_word type = ir[++i];
				spv_word loc = ir[++i];

				std::string varName = names[loc];

				if (curFunc.empty()) {
					spvgentwo::spv::StorageClass sType = (spvgentwo::spv::StorageClass)ir[++i];
					if (sType == spvgentwo::spv::StorageClass::Uniform || sType == spvgentwo::spv::StorageClass::UniformConstant) {
						Variable uni;
						uni.Name = varName;
						fetchType(uni, type);

						if (uni.Name.size() == 0 || uni.Name[0] == 0) {
							if (UserTypes.count(uni.TypeName) > 0) {
								const std::vector<Variable>& mems = UserTypes[uni.TypeName];
								for (const auto& mem : mems)
									Uniforms.push_back(mem);
							}
						} else
							Uniforms.push_back(uni);
					} else if (varName.size() > 0 && varName[0] != 0) {
						Variable glob;
						glob.Name = varName;
						fetchType(glob, type);

						Globals.push_back(glob);
					}
				} else {
					Variable loc;
					loc.Name = varName;
					fetchType(loc, type);
					Functions[curFunc].Locals.push_back(loc);
				}
			} break;
			case spvgentwo::spv::Op::OpFunctionParameter: {
				spv_word type = ir[++i];
				spv_word loc = ir[++i];

				Variable arg;
				arg.Name = names[loc];
				fetchType(arg, type);
				Functions[curFunc].Arguments.push_back(arg);
			} break;
			case spvgentwo::spv::Op::OpTypePointer: {
				spv_word loc = ir[++i];
				++i; // skip storage class
				spv_word type = ir[++i];

				pointers[loc] = type;
			} break;
			case spvgentwo::spv::Op::OpTypeBool: {
				spv_word loc = ir[++i];
				types[loc] = std::make_pair(ValueType::Bool, 0);
			} break;
			case spvgentwo::spv::Op::OpTypeInt: {
				spv_word loc = ir[++i];
				types[loc] = std::make_pair(ValueType::Int, 0);
			} break;
			case spvgentwo::spv::Op::OpTypeFloat: {
				spv_word loc = ir[++i];
				types[loc] = std::make_pair(ValueType::Float, 0);
			} break;
			case spvgentwo::spv::Op::OpTypeVector: {
				spv_word loc = ir[++i];
				spv_word comp = ir[++i];
				spv_word compcount = ir[++i];

				spv_word val = (compcount & 0x00FFFFFF) | (((spv_word)types[comp].first) << 24);

				types[loc] = std::make_pair(ValueType::Vector, val);
			} break;
			case spvgentwo::spv::Op::OpTypeMatrix: {
				spv_word loc = ir[++i];
				spv_word comp = ir[++i];
				spv_word compcount = ir[++i];

				spv_word val = (compcount & 0x00FFFFFF) | (types[comp].second & 0xFF000000);

				types[loc] = std::make_pair(ValueType::Matrix, val);
			} break;
			case spvgentwo::spv::Op::OpExecutionMode: {
				++i; // skip
				spvgentwo::spv::ExecutionMode execMode = (spvgentwo::spv::ExecutionMode)ir[++i];

				if (execMode == spvgentwo::spv::ExecutionMode::LocalSize) {
					LocalSizeX = ir[++i];
					LocalSizeY = ir[++i];
					LocalSizeZ = ir[++i];
				}
			} break;

			case spvgentwo::spv::Op::OpControlBarrier:
			case spvgentwo::spv::Op::OpMemoryBarrier:
			case spvgentwo::spv::Op::OpNamedBarrierInitialize: {
				BarrierUsed = true;
			} break;

			case spvgentwo::spv::Op::OpSNegate: case spvgentwo::spv::Op::OpFNegate:
			case spvgentwo::spv::Op::OpIAdd: case spvgentwo::spv::Op::OpFAdd:
			case spvgentwo::spv::Op::OpISub: case spvgentwo::spv::Op::OpFSub:
			case spvgentwo::spv::Op::OpIMul: case spvgentwo::spv::Op::OpFMul:
			case spvgentwo::spv::Op::OpUDiv: case spvgentwo::spv::Op::OpSDiv:
			case spvgentwo::spv::Op::OpFDiv: case spvgentwo::spv::Op::OpUMod:
			case spvgentwo::spv::Op::OpSRem: case spvgentwo::spv::Op::OpSMod:
			case spvgentwo::spv::Op::OpFRem: case spvgentwo::spv::Op::OpFMod:
			case spvgentwo::spv::Op::OpVectorTimesScalar:
			case spvgentwo::spv::Op::OpMatrixTimesScalar:
			case spvgentwo::spv::Op::OpVectorTimesMatrix:
			case spvgentwo::spv::Op::OpMatrixTimesVector:
			case spvgentwo::spv::Op::OpMatrixTimesMatrix:
			case spvgentwo::spv::Op::OpOuterProduct:
			case spvgentwo::spv::Op::OpDot:
			case spvgentwo::spv::Op::OpIAddCarry:
			case spvgentwo::spv::Op::OpISubBorrow:
			case spvgentwo::spv::Op::OpUMulExtended:
			case spvgentwo::spv::Op::OpSMulExtended:
				ArithmeticInstCount++;
				break;

				
			case spvgentwo::spv::Op::OpShiftRightLogical:
			case spvgentwo::spv::Op::OpShiftRightArithmetic:
			case spvgentwo::spv::Op::OpShiftLeftLogical:
			case spvgentwo::spv::Op::OpBitwiseOr:
			case spvgentwo::spv::Op::OpBitwiseXor:
			case spvgentwo::spv::Op::OpBitwiseAnd:
			case spvgentwo::spv::Op::OpNot:
			case spvgentwo::spv::Op::OpBitFieldInsert:
			case spvgentwo::spv::Op::OpBitFieldSExtract:
			case spvgentwo::spv::Op::OpBitFieldUExtract:
			case spvgentwo::spv::Op::OpBitReverse:
			case spvgentwo::spv::Op::OpBitCount:
				BitInstCount++;
				break;

			case spvgentwo::spv::Op::OpAny: case spvgentwo::spv::Op::OpAll:
			case spvgentwo::spv::Op::OpIsNan: case spvgentwo::spv::Op::OpIsInf:
			case spvgentwo::spv::Op::OpIsFinite: case spvgentwo::spv::Op::OpIsNormal:
			case spvgentwo::spv::Op::OpSignBitSet: case spvgentwo::spv::Op::OpLessOrGreater:
			case spvgentwo::spv::Op::OpOrdered: case spvgentwo::spv::Op::OpUnordered:
			case spvgentwo::spv::Op::OpLogicalEqual: case spvgentwo::spv::Op::OpLogicalNotEqual:
			case spvgentwo::spv::Op::OpLogicalOr: case spvgentwo::spv::Op::OpLogicalAnd:
			case spvgentwo::spv::Op::OpLogicalNot: case spvgentwo::spv::Op::OpSelect:
			case spvgentwo::spv::Op::OpIEqual: case spvgentwo::spv::Op::OpINotEqual:
			case spvgentwo::spv::Op::OpUGreaterThan: case spvgentwo::spv::Op::OpSGreaterThan:
			case spvgentwo::spv::Op::OpUGreaterThanEqual: case spvgentwo::spv::Op::OpSGreaterThanEqual:
			case spvgentwo::spv::Op::OpULessThan: case spvgentwo::spv::Op::OpSLessThan:
			case spvgentwo::spv::Op::OpULessThanEqual: case spvgentwo::spv::Op::OpSLessThanEqual:
			case spvgentwo::spv::Op::OpFOrdEqual: case spvgentwo::spv::Op::OpFUnordEqual:
			case spvgentwo::spv::Op::OpFOrdNotEqual: case spvgentwo::spv::Op::OpFUnordNotEqual:
			case spvgentwo::spv::Op::OpFOrdLessThan: case spvgentwo::spv::Op::OpFUnordLessThan:
			case spvgentwo::spv::Op::OpFOrdGreaterThan: case spvgentwo::spv::Op::OpFUnordGreaterThan:
			case spvgentwo::spv::Op::OpFOrdLessThanEqual: case spvgentwo::spv::Op::OpFUnordLessThanEqual:
			case spvgentwo::spv::Op::OpFOrdGreaterThanEqual: case spvgentwo::spv::Op::OpFUnordGreaterThanEqual:
				LogicalInstCount++;
				break;

			case spvgentwo::spv::Op::OpImageSampleImplicitLod:
			case spvgentwo::spv::Op::OpImageSampleExplicitLod:
			case spvgentwo::spv::Op::OpImageSampleDrefImplicitLod:
			case spvgentwo::spv::Op::OpImageSampleDrefExplicitLod:
			case spvgentwo::spv::Op::OpImageSampleProjImplicitLod:
			case spvgentwo::spv::Op::OpImageSampleProjExplicitLod:
			case spvgentwo::spv::Op::OpImageSampleProjDrefImplicitLod:
			case spvgentwo::spv::Op::OpImageSampleProjDrefExplicitLod:
			case spvgentwo::spv::Op::OpImageFetch: case spvgentwo::spv::Op::OpImageGather:
			case spvgentwo::spv::Op::OpImageDrefGather: case spvgentwo::spv::Op::OpImageRead:
			case spvgentwo::spv::Op::OpImageWrite:
				TextureInstCount++;
				break;

			case spvgentwo::spv::Op::OpDPdx:
			case spvgentwo::spv::Op::OpDPdy:
			case spvgentwo::spv::Op::OpFwidth:
			case spvgentwo::spv::Op::OpDPdxFine:
			case spvgentwo::spv::Op::OpDPdyFine:
			case spvgentwo::spv::Op::OpFwidthFine:
			case spvgentwo::spv::Op::OpDPdxCoarse:
			case spvgentwo::spv::Op::OpDPdyCoarse:
			case spvgentwo::spv::Op::OpFwidthCoarse:
				DerivativeInstCount++;
				break;

			case spvgentwo::spv::Op::OpPhi:
			case spvgentwo::spv::Op::OpLoopMerge:
			case spvgentwo::spv::Op::OpSelectionMerge:
			case spvgentwo::spv::Op::OpLabel:
			case spvgentwo::spv::Op::OpBranch:
			case spvgentwo::spv::Op::OpBranchConditional:
			case spvgentwo::spv::Op::OpSwitch:
			case spvgentwo::spv::Op::OpKill:
			case spvgentwo::spv::Op::OpReturn:
			case spvgentwo::spv::Op::OpReturnValue:
				ControlFlowInstCount++;
				break;
			case spvgentwo::spv::Op::OpNop: break;
			case spvgentwo::spv::Op::OpUndef: break;
			case spvgentwo::spv::Op::OpSourceContinued: break;
			case spvgentwo::spv::Op::OpSource: break;
			case spvgentwo::spv::Op::OpSourceExtension: break;
			case spvgentwo::spv::Op::OpString: break;
			case spvgentwo::spv::Op::OpExtension: break;
			case spvgentwo::spv::Op::OpExtInstImport: break;
			case spvgentwo::spv::Op::OpExtInst: break;
			case spvgentwo::spv::Op::OpMemoryModel: break;
			case spvgentwo::spv::Op::OpEntryPoint: break;
			case spvgentwo::spv::Op::OpCapability: break;
			case spvgentwo::spv::Op::OpTypeVoid: break;
			case spvgentwo::spv::Op::OpTypeImage: break;
			case spvgentwo::spv::Op::OpTypeSampler: break;
			case spvgentwo::spv::Op::OpTypeSampledImage: break;
			case spvgentwo::spv::Op::OpTypeArray: break;
			case spvgentwo::spv::Op::OpTypeRuntimeArray: break;
			case spvgentwo::spv::Op::OpTypeOpaque: break;
			case spvgentwo::spv::Op::OpTypeFunction: break;
			case spvgentwo::spv::Op::OpTypeEvent: break;
			case spvgentwo::spv::Op::OpTypeDeviceEvent: break;
			case spvgentwo::spv::Op::OpTypeReserveId: break;
			case spvgentwo::spv::Op::OpTypeQueue: break;
			case spvgentwo::spv::Op::OpTypePipe: break;
			case spvgentwo::spv::Op::OpTypeForwardPointer: break;
			case spvgentwo::spv::Op::OpConstantTrue: break;
			case spvgentwo::spv::Op::OpConstantFalse: break;
			case spvgentwo::spv::Op::OpConstant: break;
			case spvgentwo::spv::Op::OpConstantComposite: break;
			case spvgentwo::spv::Op::OpConstantSampler: break;
			case spvgentwo::spv::Op::OpConstantNull: break;
			case spvgentwo::spv::Op::OpSpecConstantTrue: break;
			case spvgentwo::spv::Op::OpSpecConstantFalse: break;
			case spvgentwo::spv::Op::OpSpecConstant: break;
			case spvgentwo::spv::Op::OpSpecConstantComposite: break;
			case spvgentwo::spv::Op::OpSpecConstantOp: break;
			case spvgentwo::spv::Op::OpFunctionCall: break;
			case spvgentwo::spv::Op::OpImageTexelPointer: break;
			case spvgentwo::spv::Op::OpLoad: break;
			case spvgentwo::spv::Op::OpStore: break;
			case spvgentwo::spv::Op::OpCopyMemory: break;
			case spvgentwo::spv::Op::OpCopyMemorySized: break;
			case spvgentwo::spv::Op::OpAccessChain: break;
			case spvgentwo::spv::Op::OpInBoundsAccessChain: break;
			case spvgentwo::spv::Op::OpPtrAccessChain: break;
			case spvgentwo::spv::Op::OpArrayLength: break;
			case spvgentwo::spv::Op::OpGenericPtrMemSemantics: break;
			case spvgentwo::spv::Op::OpInBoundsPtrAccessChain: break;
			case spvgentwo::spv::Op::OpDecorate: break;
			case spvgentwo::spv::Op::OpMemberDecorate: break;
			case spvgentwo::spv::Op::OpDecorationGroup: break;
			case spvgentwo::spv::Op::OpGroupDecorate: break;
			case spvgentwo::spv::Op::OpGroupMemberDecorate: break;
			case spvgentwo::spv::Op::OpVectorExtractDynamic: break;
			case spvgentwo::spv::Op::OpVectorInsertDynamic: break;
			case spvgentwo::spv::Op::OpVectorShuffle: break;
			case spvgentwo::spv::Op::OpCompositeConstruct: break;
			case spvgentwo::spv::Op::OpCompositeExtract: break;
			case spvgentwo::spv::Op::OpCompositeInsert: break;
			case spvgentwo::spv::Op::OpCopyObject: break;
			case spvgentwo::spv::Op::OpTranspose: break;
			case spvgentwo::spv::Op::OpSampledImage: break;
			case spvgentwo::spv::Op::OpImage: break;
			case spvgentwo::spv::Op::OpImageQueryFormat: break;
			case spvgentwo::spv::Op::OpImageQueryOrder: break;
			case spvgentwo::spv::Op::OpImageQuerySizeLod: break;
			case spvgentwo::spv::Op::OpImageQuerySize: break;
			case spvgentwo::spv::Op::OpImageQueryLod: break;
			case spvgentwo::spv::Op::OpImageQueryLevels: break;
			case spvgentwo::spv::Op::OpImageQuerySamples: break;
			case spvgentwo::spv::Op::OpConvertFToU: break;
			case spvgentwo::spv::Op::OpConvertFToS: break;
			case spvgentwo::spv::Op::OpConvertSToF: break;
			case spvgentwo::spv::Op::OpConvertUToF: break;
			case spvgentwo::spv::Op::OpUConvert: break;
			case spvgentwo::spv::Op::OpSConvert: break;
			case spvgentwo::spv::Op::OpFConvert: break;
			case spvgentwo::spv::Op::OpQuantizeToF16: break;
			case spvgentwo::spv::Op::OpConvertPtrToU: break;
			case spvgentwo::spv::Op::OpSatConvertSToU: break;
			case spvgentwo::spv::Op::OpSatConvertUToS: break;
			case spvgentwo::spv::Op::OpConvertUToPtr: break;
			case spvgentwo::spv::Op::OpPtrCastToGeneric: break;
			case spvgentwo::spv::Op::OpGenericCastToPtr: break;
			case spvgentwo::spv::Op::OpGenericCastToPtrExplicit: break;
			case spvgentwo::spv::Op::OpBitcast: break;
			case spvgentwo::spv::Op::OpEmitVertex: break;
			case spvgentwo::spv::Op::OpEndPrimitive: break;
			case spvgentwo::spv::Op::OpEmitStreamVertex: break;
			case spvgentwo::spv::Op::OpEndStreamPrimitive: break;
			case spvgentwo::spv::Op::OpAtomicLoad: break;
			case spvgentwo::spv::Op::OpAtomicStore: break;
			case spvgentwo::spv::Op::OpAtomicExchange: break;
			case spvgentwo::spv::Op::OpAtomicCompareExchange: break;
			case spvgentwo::spv::Op::OpAtomicCompareExchangeWeak: break;
			case spvgentwo::spv::Op::OpAtomicIIncrement: break;
			case spvgentwo::spv::Op::OpAtomicIDecrement: break;
			case spvgentwo::spv::Op::OpAtomicIAdd: break;
			case spvgentwo::spv::Op::OpAtomicISub: break;
			case spvgentwo::spv::Op::OpAtomicSMin: break;
			case spvgentwo::spv::Op::OpAtomicUMin: break;
			case spvgentwo::spv::Op::OpAtomicSMax: break;
			case spvgentwo::spv::Op::OpAtomicUMax: break;
			case spvgentwo::spv::Op::OpAtomicAnd: break;
			case spvgentwo::spv::Op::OpAtomicOr: break;
			case spvgentwo::spv::Op::OpAtomicXor: break;
			case spvgentwo::spv::Op::OpUnreachable: break;
			case spvgentwo::spv::Op::OpLifetimeStart: break;
			case spvgentwo::spv::Op::OpLifetimeStop: break;
			case spvgentwo::spv::Op::OpGroupAsyncCopy: break;
			case spvgentwo::spv::Op::OpGroupWaitEvents: break;
			case spvgentwo::spv::Op::OpGroupAll: break;
			case spvgentwo::spv::Op::OpGroupAny: break;
			case spvgentwo::spv::Op::OpGroupBroadcast: break;
			case spvgentwo::spv::Op::OpGroupIAdd: break;
			case spvgentwo::spv::Op::OpGroupFAdd: break;
			case spvgentwo::spv::Op::OpGroupFMin: break;
			case spvgentwo::spv::Op::OpGroupUMin: break;
			case spvgentwo::spv::Op::OpGroupSMin: break;
			case spvgentwo::spv::Op::OpGroupFMax: break;
			case spvgentwo::spv::Op::OpGroupUMax: break;
			case spvgentwo::spv::Op::OpGroupSMax: break;
			case spvgentwo::spv::Op::OpReadPipe: break;
			case spvgentwo::spv::Op::OpWritePipe: break;
			case spvgentwo::spv::Op::OpReservedReadPipe: break;
			case spvgentwo::spv::Op::OpReservedWritePipe: break;
			case spvgentwo::spv::Op::OpReserveReadPipePackets: break;
			case spvgentwo::spv::Op::OpReserveWritePipePackets: break;
			case spvgentwo::spv::Op::OpCommitReadPipe: break;
			case spvgentwo::spv::Op::OpCommitWritePipe: break;
			case spvgentwo::spv::Op::OpIsValidReserveId: break;
			case spvgentwo::spv::Op::OpGetNumPipePackets: break;
			case spvgentwo::spv::Op::OpGetMaxPipePackets: break;
			case spvgentwo::spv::Op::OpGroupReserveReadPipePackets: break;
			case spvgentwo::spv::Op::OpGroupReserveWritePipePackets: break;
			case spvgentwo::spv::Op::OpGroupCommitReadPipe: break;
			case spvgentwo::spv::Op::OpGroupCommitWritePipe: break;
			case spvgentwo::spv::Op::OpEnqueueMarker: break;
			case spvgentwo::spv::Op::OpEnqueueKernel: break;
			case spvgentwo::spv::Op::OpGetKernelNDrangeSubGroupCount: break;
			case spvgentwo::spv::Op::OpGetKernelNDrangeMaxSubGroupSize: break;
			case spvgentwo::spv::Op::OpGetKernelWorkGroupSize: break;
			case spvgentwo::spv::Op::OpGetKernelPreferredWorkGroupSizeMultiple: break;
			case spvgentwo::spv::Op::OpRetainEvent: break;
			case spvgentwo::spv::Op::OpReleaseEvent: break;
			case spvgentwo::spv::Op::OpCreateUserEvent: break;
			case spvgentwo::spv::Op::OpIsValidEvent: break;
			case spvgentwo::spv::Op::OpSetUserEventStatus: break;
			case spvgentwo::spv::Op::OpCaptureEventProfilingInfo: break;
			case spvgentwo::spv::Op::OpGetDefaultQueue: break;
			case spvgentwo::spv::Op::OpBuildNDRange: break;
			case spvgentwo::spv::Op::OpImageSparseSampleImplicitLod: break;
			case spvgentwo::spv::Op::OpImageSparseSampleExplicitLod: break;
			case spvgentwo::spv::Op::OpImageSparseSampleDrefImplicitLod: break;
			case spvgentwo::spv::Op::OpImageSparseSampleDrefExplicitLod: break;
			case spvgentwo::spv::Op::OpImageSparseSampleProjImplicitLod: break;
			case spvgentwo::spv::Op::OpImageSparseSampleProjExplicitLod: break;
			case spvgentwo::spv::Op::OpImageSparseSampleProjDrefImplicitLod: break;
			case spvgentwo::spv::Op::OpImageSparseSampleProjDrefExplicitLod: break;
			case spvgentwo::spv::Op::OpImageSparseFetch: break;
			case spvgentwo::spv::Op::OpImageSparseGather: break;
			case spvgentwo::spv::Op::OpImageSparseDrefGather: break;
			case spvgentwo::spv::Op::OpImageSparseTexelsResident: break;
			case spvgentwo::spv::Op::OpNoLine: break;
			case spvgentwo::spv::Op::OpAtomicFlagTestAndSet: break;
			case spvgentwo::spv::Op::OpAtomicFlagClear: break;
			case spvgentwo::spv::Op::OpImageSparseRead: break;
			case spvgentwo::spv::Op::OpSizeOf: break;
			case spvgentwo::spv::Op::OpTypePipeStorage: break;
			case spvgentwo::spv::Op::OpConstantPipeStorage: break;
			case spvgentwo::spv::Op::OpCreatePipeFromPipeStorage: break;
			case spvgentwo::spv::Op::OpGetKernelLocalSizeForSubgroupCount: break;
			case spvgentwo::spv::Op::OpGetKernelMaxNumSubgroups: break;
			case spvgentwo::spv::Op::OpTypeNamedBarrier: break;
			case spvgentwo::spv::Op::OpMemoryNamedBarrier: break;
			case spvgentwo::spv::Op::OpModuleProcessed: break;
			case spvgentwo::spv::Op::OpExecutionModeId: break;
			case spvgentwo::spv::Op::OpDecorateId: break;
			case spvgentwo::spv::Op::OpGroupNonUniformElect: break;
			case spvgentwo::spv::Op::OpGroupNonUniformAll: break;
			case spvgentwo::spv::Op::OpGroupNonUniformAny: break;
			case spvgentwo::spv::Op::OpGroupNonUniformAllEqual: break;
			case spvgentwo::spv::Op::OpGroupNonUniformBroadcast: break;
			case spvgentwo::spv::Op::OpGroupNonUniformBroadcastFirst: break;
			case spvgentwo::spv::Op::OpGroupNonUniformBallot: break;
			case spvgentwo::spv::Op::OpGroupNonUniformInverseBallot: break;
			case spvgentwo::spv::Op::OpGroupNonUniformBallotBitExtract: break;
			case spvgentwo::spv::Op::OpGroupNonUniformBallotBitCount: break;
			case spvgentwo::spv::Op::OpGroupNonUniformBallotFindLSB: break;
			case spvgentwo::spv::Op::OpGroupNonUniformBallotFindMSB: break;
			case spvgentwo::spv::Op::OpGroupNonUniformShuffle: break;
			case spvgentwo::spv::Op::OpGroupNonUniformShuffleXor: break;
			case spvgentwo::spv::Op::OpGroupNonUniformShuffleUp: break;
			case spvgentwo::spv::Op::OpGroupNonUniformShuffleDown: break;
			case spvgentwo::spv::Op::OpGroupNonUniformIAdd: break;
			case spvgentwo::spv::Op::OpGroupNonUniformFAdd: break;
			case spvgentwo::spv::Op::OpGroupNonUniformIMul: break;
			case spvgentwo::spv::Op::OpGroupNonUniformFMul: break;
			case spvgentwo::spv::Op::OpGroupNonUniformSMin: break;
			case spvgentwo::spv::Op::OpGroupNonUniformUMin: break;
			case spvgentwo::spv::Op::OpGroupNonUniformFMin: break;
			case spvgentwo::spv::Op::OpGroupNonUniformSMax: break;
			case spvgentwo::spv::Op::OpGroupNonUniformUMax: break;
			case spvgentwo::spv::Op::OpGroupNonUniformFMax: break;
			case spvgentwo::spv::Op::OpGroupNonUniformBitwiseAnd: break;
			case spvgentwo::spv::Op::OpGroupNonUniformBitwiseOr: break;
			case spvgentwo::spv::Op::OpGroupNonUniformBitwiseXor: break;
			case spvgentwo::spv::Op::OpGroupNonUniformLogicalAnd: break;
			case spvgentwo::spv::Op::OpGroupNonUniformLogicalOr: break;
			case spvgentwo::spv::Op::OpGroupNonUniformLogicalXor: break;
			case spvgentwo::spv::Op::OpGroupNonUniformQuadBroadcast: break;
			case spvgentwo::spv::Op::OpGroupNonUniformQuadSwap: break;
			case spvgentwo::spv::Op::OpCopyLogical: break;
			case spvgentwo::spv::Op::OpPtrEqual: break;
			case spvgentwo::spv::Op::OpPtrNotEqual: break;
			case spvgentwo::spv::Op::OpPtrDiff: break;
			case spvgentwo::spv::Op::OpTerminateInvocation: break;
			case spvgentwo::spv::Op::OpSubgroupBallotKHR: break;
			case spvgentwo::spv::Op::OpSubgroupFirstInvocationKHR: break;
			case spvgentwo::spv::Op::OpSubgroupAllKHR: break;
			case spvgentwo::spv::Op::OpSubgroupAnyKHR: break;
			case spvgentwo::spv::Op::OpSubgroupAllEqualKHR: break;
			case spvgentwo::spv::Op::OpSubgroupReadInvocationKHR: break;
			case spvgentwo::spv::Op::OpTraceRayKHR: break;
			case spvgentwo::spv::Op::OpExecuteCallableKHR: break;
			case spvgentwo::spv::Op::OpConvertUToAccelerationStructureKHR: break;
			case spvgentwo::spv::Op::OpIgnoreIntersectionKHR: break;
			case spvgentwo::spv::Op::OpTerminateRayKHR: break;
			case spvgentwo::spv::Op::OpTypeRayQueryKHR: break;
			case spvgentwo::spv::Op::OpRayQueryInitializeKHR: break;
			case spvgentwo::spv::Op::OpRayQueryTerminateKHR: break;
			case spvgentwo::spv::Op::OpRayQueryGenerateIntersectionKHR: break;
			case spvgentwo::spv::Op::OpRayQueryConfirmIntersectionKHR: break;
			case spvgentwo::spv::Op::OpRayQueryProceedKHR: break;
			case spvgentwo::spv::Op::OpRayQueryGetIntersectionTypeKHR: break;
			case spvgentwo::spv::Op::OpGroupIAddNonUniformAMD: break;
			case spvgentwo::spv::Op::OpGroupFAddNonUniformAMD: break;
			case spvgentwo::spv::Op::OpGroupFMinNonUniformAMD: break;
			case spvgentwo::spv::Op::OpGroupUMinNonUniformAMD: break;
			case spvgentwo::spv::Op::OpGroupSMinNonUniformAMD: break;
			case spvgentwo::spv::Op::OpGroupFMaxNonUniformAMD: break;
			case spvgentwo::spv::Op::OpGroupUMaxNonUniformAMD: break;
			case spvgentwo::spv::Op::OpGroupSMaxNonUniformAMD: break;
			case spvgentwo::spv::Op::OpFragmentMaskFetchAMD: break;
			case spvgentwo::spv::Op::OpFragmentFetchAMD: break;
			case spvgentwo::spv::Op::OpReadClockKHR: break;
			case spvgentwo::spv::Op::OpImageSampleFootprintNV: break;
			case spvgentwo::spv::Op::OpGroupNonUniformPartitionNV: break;
			case spvgentwo::spv::Op::OpWritePackedPrimitiveIndices4x8NV: break;
			case spvgentwo::spv::Op::OpReportIntersectionKHR: break;
			case spvgentwo::spv::Op::OpIgnoreIntersectionNV: break;
			case spvgentwo::spv::Op::OpTerminateRayNV: break;
			case spvgentwo::spv::Op::OpTraceNV: break;
			case spvgentwo::spv::Op::OpTypeAccelerationStructureNV: break;
			case spvgentwo::spv::Op::OpExecuteCallableNV: break;
			case spvgentwo::spv::Op::OpTypeCooperativeMatrixNV: break;
			case spvgentwo::spv::Op::OpCooperativeMatrixLoadNV: break;
			case spvgentwo::spv::Op::OpCooperativeMatrixStoreNV: break;
			case spvgentwo::spv::Op::OpCooperativeMatrixMulAddNV: break;
			case spvgentwo::spv::Op::OpCooperativeMatrixLengthNV: break;
			case spvgentwo::spv::Op::OpBeginInvocationInterlockEXT: break;
			case spvgentwo::spv::Op::OpEndInvocationInterlockEXT: break;
			case spvgentwo::spv::Op::OpDemoteToHelperInvocationEXT: break;
			case spvgentwo::spv::Op::OpIsHelperInvocationEXT: break;
			case spvgentwo::spv::Op::OpSubgroupShuffleINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupShuffleDownINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupShuffleUpINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupShuffleXorINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupBlockReadINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupBlockWriteINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupImageBlockReadINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupImageBlockWriteINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupImageMediaBlockReadINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupImageMediaBlockWriteINTEL: break;
			case spvgentwo::spv::Op::OpUCountLeadingZerosINTEL: break;
			case spvgentwo::spv::Op::OpUCountTrailingZerosINTEL: break;
			case spvgentwo::spv::Op::OpAbsISubINTEL: break;
			case spvgentwo::spv::Op::OpAbsUSubINTEL: break;
			case spvgentwo::spv::Op::OpIAddSatINTEL: break;
			case spvgentwo::spv::Op::OpUAddSatINTEL: break;
			case spvgentwo::spv::Op::OpIAverageINTEL: break;
			case spvgentwo::spv::Op::OpUAverageINTEL: break;
			case spvgentwo::spv::Op::OpIAverageRoundedINTEL: break;
			case spvgentwo::spv::Op::OpUAverageRoundedINTEL: break;
			case spvgentwo::spv::Op::OpISubSatINTEL: break;
			case spvgentwo::spv::Op::OpUSubSatINTEL: break;
			case spvgentwo::spv::Op::OpIMul32x16INTEL: break;
			case spvgentwo::spv::Op::OpUMul32x16INTEL: break;
			case spvgentwo::spv::Op::OpConstFunctionPointerINTEL: break;
			case spvgentwo::spv::Op::OpFunctionPointerCallINTEL: break;
			case spvgentwo::spv::Op::OpAsmTargetINTEL: break;
			case spvgentwo::spv::Op::OpAsmINTEL: break;
			case spvgentwo::spv::Op::OpAsmCallINTEL: break;
			case spvgentwo::spv::Op::OpAtomicFMinEXT: break;
			case spvgentwo::spv::Op::OpAtomicFMaxEXT: break;
			case spvgentwo::spv::Op::OpDecorateString: break;
			case spvgentwo::spv::Op::OpMemberDecorateString: break;
			case spvgentwo::spv::Op::OpVmeImageINTEL: break;
			case spvgentwo::spv::Op::OpTypeVmeImageINTEL: break;
			case spvgentwo::spv::Op::OpTypeAvcImePayloadINTEL: break;
			case spvgentwo::spv::Op::OpTypeAvcRefPayloadINTEL: break;
			case spvgentwo::spv::Op::OpTypeAvcSicPayloadINTEL: break;
			case spvgentwo::spv::Op::OpTypeAvcMcePayloadINTEL: break;
			case spvgentwo::spv::Op::OpTypeAvcMceResultINTEL: break;
			case spvgentwo::spv::Op::OpTypeAvcImeResultINTEL: break;
			case spvgentwo::spv::Op::OpTypeAvcImeResultSingleReferenceStreamoutINTEL: break;
			case spvgentwo::spv::Op::OpTypeAvcImeResultDualReferenceStreamoutINTEL: break;
			case spvgentwo::spv::Op::OpTypeAvcImeSingleReferenceStreaminINTEL: break;
			case spvgentwo::spv::Op::OpTypeAvcImeDualReferenceStreaminINTEL: break;
			case spvgentwo::spv::Op::OpTypeAvcRefResultINTEL: break;
			case spvgentwo::spv::Op::OpTypeAvcSicResultINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetDefaultInterBaseMultiReferencePenaltyINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceSetInterBaseMultiReferencePenaltyINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetDefaultInterShapePenaltyINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceSetInterShapePenaltyINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetDefaultInterDirectionPenaltyINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceSetInterDirectionPenaltyINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetDefaultIntraLumaShapePenaltyINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetDefaultInterMotionVectorCostTableINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetDefaultHighPenaltyCostTableINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetDefaultMediumPenaltyCostTableINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetDefaultLowPenaltyCostTableINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceSetMotionVectorCostFunctionINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetDefaultIntraLumaModePenaltyINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetDefaultNonDcLumaIntraPenaltyINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetDefaultIntraChromaModeBasePenaltyINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceSetAcOnlyHaarINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceSetSourceInterlacedFieldPolarityINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceSetSingleReferenceInterlacedFieldPolarityINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceSetDualReferenceInterlacedFieldPolaritiesINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceConvertToImePayloadINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceConvertToImeResultINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceConvertToRefPayloadINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceConvertToRefResultINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceConvertToSicPayloadINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceConvertToSicResultINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetMotionVectorsINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetInterDistortionsINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetBestInterDistortionsINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetInterMajorShapeINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetInterMinorShapeINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetInterDirectionsINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetInterMotionVectorCountINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetInterReferenceIdsINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcMceGetInterReferenceInterlacedFieldPolaritiesINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeInitializeINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeSetSingleReferenceINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeSetDualReferenceINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeRefWindowSizeINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeAdjustRefOffsetINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeConvertToMcePayloadINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeSetMaxMotionVectorCountINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeSetUnidirectionalMixDisableINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeSetEarlySearchTerminationThresholdINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeSetWeightedSadINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeEvaluateWithSingleReferenceINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeEvaluateWithDualReferenceINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeEvaluateWithSingleReferenceStreaminINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeEvaluateWithDualReferenceStreaminINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeEvaluateWithSingleReferenceStreamoutINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeEvaluateWithDualReferenceStreamoutINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeEvaluateWithSingleReferenceStreaminoutINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeEvaluateWithDualReferenceStreaminoutINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeConvertToMceResultINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeGetSingleReferenceStreaminINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeGetDualReferenceStreaminINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeStripSingleReferenceStreamoutINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeStripDualReferenceStreamoutINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeGetStreamoutSingleReferenceMajorShapeMotionVectorsINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeGetStreamoutSingleReferenceMajorShapeDistortionsINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeGetStreamoutSingleReferenceMajorShapeReferenceIdsINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeGetStreamoutDualReferenceMajorShapeMotionVectorsINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeGetStreamoutDualReferenceMajorShapeDistortionsINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeGetStreamoutDualReferenceMajorShapeReferenceIdsINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeGetBorderReachedINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeGetTruncatedSearchIndicationINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeGetUnidirectionalEarlySearchTerminationINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeGetWeightingPatternMinimumMotionVectorINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcImeGetWeightingPatternMinimumDistortionINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcFmeInitializeINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcBmeInitializeINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcRefConvertToMcePayloadINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcRefSetBidirectionalMixDisableINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcRefSetBilinearFilterEnableINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcRefEvaluateWithSingleReferenceINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcRefEvaluateWithDualReferenceINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcRefEvaluateWithMultiReferenceINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcRefEvaluateWithMultiReferenceInterlacedINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcRefConvertToMceResultINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicInitializeINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicConfigureSkcINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicConfigureIpeLumaINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicConfigureIpeLumaChromaINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicGetMotionVectorMaskINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicConvertToMcePayloadINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicSetIntraLumaShapePenaltyINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicSetIntraLumaModeCostFunctionINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicSetIntraChromaModeCostFunctionINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicSetBilinearFilterEnableINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicSetSkcForwardTransformEnableINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicSetBlockBasedRawSkipSadINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicEvaluateIpeINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicEvaluateWithSingleReferenceINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicEvaluateWithDualReferenceINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicEvaluateWithMultiReferenceINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicEvaluateWithMultiReferenceInterlacedINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicConvertToMceResultINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicGetIpeLumaShapeINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicGetBestIpeLumaDistortionINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicGetBestIpeChromaDistortionINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicGetPackedIpeLumaModesINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicGetIpeChromaModeINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicGetPackedSkcLumaCountThresholdINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicGetPackedSkcLumaSumThresholdINTEL: break;
			case spvgentwo::spv::Op::OpSubgroupAvcSicGetInterRawSadsINTEL: break;
			case spvgentwo::spv::Op::OpVariableLengthArrayINTEL: break;
			case spvgentwo::spv::Op::OpSaveMemoryINTEL: break;
			case spvgentwo::spv::Op::OpRestoreMemoryINTEL: break;
			case spvgentwo::spv::Op::OpLoopControlINTEL: break;
			case spvgentwo::spv::Op::OpPtrCastToCrossWorkgroupINTEL: break;
			case spvgentwo::spv::Op::OpCrossWorkgroupCastToPtrINTEL: break;
			case spvgentwo::spv::Op::OpReadPipeBlockingINTEL: break;
			case spvgentwo::spv::Op::OpWritePipeBlockingINTEL: break;
			case spvgentwo::spv::Op::OpFPGARegINTEL: break;
			case spvgentwo::spv::Op::OpRayQueryGetRayTMinKHR: break;
			case spvgentwo::spv::Op::OpRayQueryGetRayFlagsKHR: break;
			case spvgentwo::spv::Op::OpRayQueryGetIntersectionTKHR: break;
			case spvgentwo::spv::Op::OpRayQueryGetIntersectionInstanceCustomIndexKHR: break;
			case spvgentwo::spv::Op::OpRayQueryGetIntersectionInstanceIdKHR: break;
			case spvgentwo::spv::Op::OpRayQueryGetIntersectionInstanceShaderBindingTableRecordOffsetKHR: break;
			case spvgentwo::spv::Op::OpRayQueryGetIntersectionGeometryIndexKHR: break;
			case spvgentwo::spv::Op::OpRayQueryGetIntersectionPrimitiveIndexKHR: break;
			case spvgentwo::spv::Op::OpRayQueryGetIntersectionBarycentricsKHR: break;
			case spvgentwo::spv::Op::OpRayQueryGetIntersectionFrontFaceKHR: break;
			case spvgentwo::spv::Op::OpRayQueryGetIntersectionCandidateAABBOpaqueKHR: break;
			case spvgentwo::spv::Op::OpRayQueryGetIntersectionObjectRayDirectionKHR: break;
			case spvgentwo::spv::Op::OpRayQueryGetIntersectionObjectRayOriginKHR: break;
			case spvgentwo::spv::Op::OpRayQueryGetWorldRayDirectionKHR: break;
			case spvgentwo::spv::Op::OpRayQueryGetWorldRayOriginKHR: break;
			case spvgentwo::spv::Op::OpRayQueryGetIntersectionObjectToWorldKHR: break;
			case spvgentwo::spv::Op::OpRayQueryGetIntersectionWorldToObjectKHR: break;
			case spvgentwo::spv::Op::OpAtomicFAddEXT: break;
			case spvgentwo::spv::Op::OpTypeBufferSurfaceINTEL: break;
			case spvgentwo::spv::Op::OpTypeStructContinuedINTEL: break;
			case spvgentwo::spv::Op::OpConstantCompositeContinuedINTEL: break;
			case spvgentwo::spv::Op::OpSpecConstantCompositeContinuedINTEL: break;
			case spvgentwo::spv::Op::Max: break;
			}

			i = iStart + wordCount + 1;
		}
	}
}