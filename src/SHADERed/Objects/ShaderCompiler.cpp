#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Objects/BinaryVectorReader.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <SHADERed/Objects/ShaderFileIncluder.h>

#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/StandAlone/DirStackFileIncluder.h>
#include <glslang/glslang/Public/ShaderLang.h>

#include <SPIRVCross/spirv_cross_util.hpp>
#include <SPIRVCross/spirv_glsl.hpp>
#include <SPIRVCross/spirv_hlsl.hpp>

#include <common/BinaryFileReader.h>
#include <common/HeapAllocator.h>
#include <common/ModulePrinter.h>
#include <spvgentwo/Grammar.h>
#include <spvgentwo/Module.h>

const TBuiltInResource DefaultTBuiltInResource = {
	/* .maxLights = */ 32,
	/* .maxClipPlanes = */ 6,
	/* .maxTextureUnits = */ 32,
	/* .maxTextureCoords = */ 32,
	/* .maxVertexAttribs = */ 64,
	/* .maxVertexUniformComponents = */ 4096,
	/* .maxVaryingFloats = */ 64,
	/* .maxVertexTextureImageUnits = */ 32,
	/* .maxCombinedTextureImageUnits = */ 80,
	/* .maxTextureImageUnits = */ 32,
	/* .maxFragmentUniformComponents = */ 4096,
	/* .maxDrawBuffers = */ 32,
	/* .maxVertexUniformVectors = */ 128,
	/* .maxVaryingVectors = */ 8,
	/* .maxFragmentUniformVectors = */ 16,
	/* .maxVertexOutputVectors = */ 16,
	/* .maxFragmentInputVectors = */ 15,
	/* .minProgramTexelOffset = */ -8,
	/* .maxProgramTexelOffset = */ 7,
	/* .maxClipDistances = */ 8,
	/* .maxComputeWorkGroupCountX = */ 65535,
	/* .maxComputeWorkGroupCountY = */ 65535,
	/* .maxComputeWorkGroupCountZ = */ 65535,
	/* .maxComputeWorkGroupSizeX = */ 1024,
	/* .maxComputeWorkGroupSizeY = */ 1024,
	/* .maxComputeWorkGroupSizeZ = */ 64,
	/* .maxComputeUniformComponents = */ 1024,
	/* .maxComputeTextureImageUnits = */ 16,
	/* .maxComputeImageUniforms = */ 8,
	/* .maxComputeAtomicCounters = */ 8,
	/* .maxComputeAtomicCounterBuffers = */ 1,
	/* .maxVaryingComponents = */ 60,
	/* .maxVertexOutputComponents = */ 64,
	/* .maxGeometryInputComponents = */ 64,
	/* .maxGeometryOutputComponents = */ 128,
	/* .maxFragmentInputComponents = */ 128,
	/* .maxImageUnits = */ 8,
	/* .maxCombinedImageUnitsAndFragmentOutputs = */ 8,
	/* .maxCombinedShaderOutputResources = */ 8,
	/* .maxImageSamples = */ 0,
	/* .maxVertexImageUniforms = */ 0,
	/* .maxTessControlImageUniforms = */ 0,
	/* .maxTessEvaluationImageUniforms = */ 0,
	/* .maxGeometryImageUniforms = */ 0,
	/* .maxFragmentImageUniforms = */ 8,
	/* .maxCombinedImageUniforms = */ 8,
	/* .maxGeometryTextureImageUnits = */ 16,
	/* .maxGeometryOutputVertices = */ 256,
	/* .maxGeometryTotalOutputComponents = */ 1024,
	/* .maxGeometryUniformComponents = */ 1024,
	/* .maxGeometryVaryingComponents = */ 64,
	/* .maxTessControlInputComponents = */ 128,
	/* .maxTessControlOutputComponents = */ 128,
	/* .maxTessControlTextureImageUnits = */ 16,
	/* .maxTessControlUniformComponents = */ 1024,
	/* .maxTessControlTotalOutputComponents = */ 4096,
	/* .maxTessEvaluationInputComponents = */ 128,
	/* .maxTessEvaluationOutputComponents = */ 128,
	/* .maxTessEvaluationTextureImageUnits = */ 16,
	/* .maxTessEvaluationUniformComponents = */ 1024,
	/* .maxTessPatchComponents = */ 120,
	/* .maxPatchVertices = */ 32,
	/* .maxTessGenLevel = */ 64,
	/* .maxViewports = */ 16,
	/* .maxVertexAtomicCounters = */ 0,
	/* .maxTessControlAtomicCounters = */ 0,
	/* .maxTessEvaluationAtomicCounters = */ 0,
	/* .maxGeometryAtomicCounters = */ 0,
	/* .maxFragmentAtomicCounters = */ 8,
	/* .maxCombinedAtomicCounters = */ 8,
	/* .maxAtomicCounterBindings = */ 1,
	/* .maxVertexAtomicCounterBuffers = */ 0,
	/* .maxTessControlAtomicCounterBuffers = */ 0,
	/* .maxTessEvaluationAtomicCounterBuffers = */ 0,
	/* .maxGeometryAtomicCounterBuffers = */ 0,
	/* .maxFragmentAtomicCounterBuffers = */ 1,
	/* .maxCombinedAtomicCounterBuffers = */ 1,
	/* .maxAtomicCounterBufferSize = */ 16384,
	/* .maxTransformFeedbackBuffers = */ 4,
	/* .maxTransformFeedbackInterleavedComponents = */ 64,
	/* .maxCullDistances = */ 8,
	/* .maxCombinedClipAndCullDistances = */ 8,
	/* .maxSamples = */ 4,

	/* .maxMeshOutputVerticesNV = */ 256,
	/* .maxMeshOutputPrimitivesNV = */ 512,
	/* .maxMeshWorkGroupSizeX_NV = */ 32,
	/* .maxMeshWorkGroupSizeY_NV = */ 1,
	/* .maxMeshWorkGroupSizeZ_NV = */ 1,
	/* .maxTaskWorkGroupSizeX_NV = */ 32,
	/* .maxTaskWorkGroupSizeY_NV = */ 1,
	/* .maxTaskWorkGroupSizeZ_NV = */ 1,
	/* .maxMeshViewCountNV = */ 4,
	/* .maxDualSourceDrawBuffersEXT = */ 1,
	/* .maxMeshOutputVerticesEXT = */ 0, // Added missing field
	/* .maxMeshOutputPrimitivesEXT = */ 256,
	/* .maxMeshWorkGroupSizeX_EXT = */ 32,
	/* .maxMeshWorkGroupSizeY_EXT = */ 1,
	/* .maxMeshWorkGroupSizeZ_EXT = */ 1,
	/* .maxTaskWorkGroupSizeX_EXT = */ 0, // Added missing field
	/* .maxTaskWorkGroupSizeY_EXT = */ 0, // Added missing field
	/* .maxTaskWorkGroupSizeZ_EXT = */ 0, // Added missing field
	/* .maxMeshViewCountEXT = */ 0,		  // Added missing field

	/* .limits = */ {
		/* .nonInductiveForLoops = */ true,
		/* .whileLoops = */ true,
		/* .doWhileLoops = */ true,
		/* .generalUniformIndexing = */ true,
		/* .generalAttributeMatrixVectorIndexing = */ true,
		/* .generalVaryingIndexing = */ true,
		/* .generalSamplerIndexing = */ true,
		/* .generalVariableIndexing = */ true,
		/* .generalConstantMatrixVectorIndexing = */ true,
	}
};

namespace ed {
	std::string ShaderCompiler::ConvertToGLSL(const std::vector<unsigned int>& spvIn, ShaderLanguage inLang, ShaderStage sType, bool tsUsed, bool gsUsed, MessageStack* msgs, bool convertNames)
	{
		if (spvIn.empty())
			return "";

		// Read SPIR-V
		spirv_cross::CompilerGLSL glsl(std::move(spvIn));

		// Set options
		spirv_cross::CompilerGLSL::Options options;
		int ver = 330;
		if (GLEW_ARB_shader_storage_buffer_object)
			ver = 430;
		options.version = (sType == ShaderStage::Compute) ? 430 : ver;
		glsl.set_common_options(options);

		// Set entry
		auto entry_points = glsl.get_entry_points_and_stages();
		spv::ExecutionModel model = spv::ExecutionModelMax;
		std::string entry_point = "";
		if (sType == ShaderStage::Vertex)
			model = spv::ExecutionModelVertex;
		else if (sType == ShaderStage::Pixel)
			model = spv::ExecutionModelFragment;
		else if (sType == ShaderStage::Geometry)
			model = spv::ExecutionModelGeometry;
		else if (sType == ShaderStage::Compute)
			model = spv::ExecutionModelGLCompute;
		else if (sType == ShaderStage::TessellationControl)
			model = spv::ExecutionModelTessellationControl;
		else if (sType == ShaderStage::TessellationEvaluation)
			model = spv::ExecutionModelTessellationEvaluation;
		for (auto& e : entry_points) {
			if (e.execution_model == model) {
				entry_point = e.name;
				break;
			}
		}
		if (!entry_point.empty() && model != spv::ExecutionModelMax)
			glsl.set_entry_point(entry_point, model);

		// rename outputs
		spirv_cross::ShaderResources resources = glsl.get_shader_resources();
		std::string outputName = "outputVS";
		if (sType == ShaderStage::TessellationControl)
			outputName = "outputTCS";
		else if (sType == ShaderStage::TessellationEvaluation)
			outputName = "outputTES";
		else if (sType == ShaderStage::Geometry)
			outputName = "outputGS";
		else if (sType == ShaderStage::Pixel)
			outputName = "outputPS";
		if (convertNames) {
			for (auto& resource : resources.stage_outputs) {
				uint32_t resID = glsl.get_decoration(resource.id, spv::DecorationLocation);
				glsl.set_name(resource.id, outputName + std::to_string(resID));
			}
		}

		// rename inputs
		std::string inputName = "inputVS";
		if (sType == ShaderStage::Pixel)
			inputName = gsUsed ? "outputGS" : (tsUsed ? "outputTES" : "outputVS");
		else if (sType == ShaderStage::Geometry)
			inputName = tsUsed ? "outputTES" : "outputVS";
		else if (sType == ShaderStage::TessellationControl)
			inputName = "outputVS";
		else if (sType == ShaderStage::TessellationEvaluation)
			inputName = "outputTCS";
		if (convertNames) {
			for (auto& resource : resources.stage_inputs) {
				uint32_t resID = glsl.get_decoration(resource.id, spv::DecorationLocation);
				glsl.set_name(resource.id, inputName + std::to_string(resID));
			}
		}

		// Compile to GLSL
		try {
			glsl.build_dummy_sampler_for_combined_images();
			glsl.build_combined_image_samplers();
		} catch (spirv_cross::CompilerError& e) {
			ed::Logger::Get().Log("An exception occured: " + std::string(e.what()), true);
			if (msgs != nullptr)
				msgs->Add(MessageStack::Type::Error, msgs->CurrentItem, "Transcompiling failed", -1, sType);
			return "error";
		}

		spirv_cross_util::inherit_combined_sampler_bindings(glsl);
		std::string source = "";
		try {
			source = glsl.compile();
		} catch (spirv_cross::CompilerError& e) {
			ed::Logger::Get().Log("Transcompiler threw an exception: " + std::string(e.what()), true);
			if (msgs != nullptr)
				msgs->Add(MessageStack::Type::Error, msgs->CurrentItem, "Transcompiling failed", -1, sType);
			return "error";
		}

		// WARNING: lots of hacks in the following code
		// remove all the UBOs when transcompiling from HLSL
		if (inLang == ShaderLanguage::HLSL) {
			std::stringstream ss(source);
			std::string line;
			source = "";
			bool inUBO = false;
			std::vector<std::string> uboNames;
			while (std::getline(ss, line)) {

				// i know, ewww, but idk if there's a function to do this (this = converting UBO
				// to separate uniforms)...
				if (line.find("layout(binding") != std::string::npos && line.find("uniform") != std::string::npos && line.find("sampler") == std::string::npos && line.find("image") == std::string::npos && line.find(" buffer ") == std::string::npos) {
					inUBO = true;
					continue;
				} else if (inUBO) {
					if (line == "{")
						continue;
					else if (line[0] == '}') {
						inUBO = false;
						uboNames.push_back(line.substr(2, line.size() - 3));
						continue;
					} else {
						size_t playout = line.find(")");
						if (playout != std::string::npos)
							line.erase(0, playout + 2);
						line = "uniform " + line;
					}
				} else { // remove all occurances of "ubo." substrings
					for (int i = 0; i < uboNames.size(); i++) {
						std::string what = uboNames[i] + ".";
						size_t n = what.length();
						for (size_t j = line.find(what); j != std::string::npos; j = line.find(what))
							line.erase(j, n);
					}
				}

				source += line + "\n";
			}
		} else if (inLang == ShaderLanguage::VulkanGLSL) {
			std::stringstream ss(source);
			std::string line;
			source = "";
			bool inUBO = false;
			std::vector<std::string> uboNames;
			int deleteUboPos = 0, deleteUboLength = 0;
			std::string uboExt = (sType == ShaderStage::Vertex) ? "VS" : (sType == ShaderStage::Pixel ? "PS" : (sType == ShaderStage::TessellationControl ? "TCS" : (sType == ShaderStage::TessellationEvaluation ? "TES" : "GS")));
			while (std::getline(ss, line)) {

				// i know, ewww, but idk if there's a function to do this (this = converting UBO
				// to separate uniforms)... TODO
				if (line.find("layout(binding") != std::string::npos && line.find("uniform") != std::string::npos && line.find("sampler") == std::string::npos && line.find("image") == std::string::npos && line.find(" buffer ") == std::string::npos) {
					inUBO = true;
					deleteUboPos = source.size();

					size_t lineLastOf = line.find_last_of(' ');
					std::string name = line.substr(lineLastOf + 1, line.find('\n') - lineLastOf - 1);
					std::string newLine = "uniform struct " + name + " {\n";
					deleteUboLength = newLine.size();
					source += newLine;
					continue;
				} else if (inUBO) {
					if (line == "{")
						continue;
					else if (line[0] == '}') {
						// i know this is yucky but are there glslang/spirv-cross functions for these? TODO
						inUBO = false;
						std::string uboName = line.substr(2, line.size() - 3);
						bool isGenBySpirvCross = false;
						if (uboName[0] == '_') {
							isGenBySpirvCross = true;
							for (int i = 1; i < uboName.size(); i++) {
								if (!isdigit(uboName[i])) {
									isGenBySpirvCross = false;
									break;
								}
							}
						}

						if (isGenBySpirvCross) {
							uboNames.push_back(uboName); // only delete occurances if the structure name is generated by spirv-cross and not us

							// delete the declaration:
							source.erase(deleteUboPos, deleteUboLength);

							for (size_t loc = deleteUboPos; loc < source.size(); loc++) {
								source.insert(loc, "uniform ");
								loc = source.find_first_of(';', loc) + 1;
							}
						} else
							source += "} " + uboName + ";\n";

						continue;
					}
					/*
					TODO: do i need to remove layout(...)?
					else
					{
						size_t playout = line.find(")");
						if (playout != std::string::npos)
							line.erase(0, playout + 2);
					}
					*/
				} else { // remove all occurances of "ubo." substrings
					for (int i = 0; i < uboNames.size(); i++) {
						std::string what = uboNames[i] + ".";
						size_t n = what.length();
						for (size_t j = line.find(what); j != std::string::npos; j = line.find(what))
							line.erase(j, n);
					}
				}

				source += line + "\n";
			}
		}

		return source;
	}
	std::string ShaderCompiler::ConvertToHLSL(const std::vector<unsigned int>& spvIn, ShaderStage sType)
	{
		if (spvIn.empty())
			return "";

		// Read SPIR-V
		spirv_cross::CompilerHLSL hlsl(std::move(spvIn));

		// Set entry
		auto entry_points = hlsl.get_entry_points_and_stages();
		spv::ExecutionModel model = spv::ExecutionModelMax;
		std::string entry_point = "";
		if (sType == ShaderStage::Vertex)
			model = spv::ExecutionModelVertex;
		else if (sType == ShaderStage::Pixel)
			model = spv::ExecutionModelFragment;
		else if (sType == ShaderStage::Geometry)
			model = spv::ExecutionModelGeometry;
		else if (sType == ShaderStage::Compute)
			model = spv::ExecutionModelGLCompute;
		else if (sType == ShaderStage::TessellationControl)
			model = spv::ExecutionModelTessellationControl;
		else if (sType == ShaderStage::TessellationEvaluation)
			model = spv::ExecutionModelTessellationEvaluation;
		for (auto& e : entry_points) {
			if (e.execution_model == model) {
				entry_point = e.name;
				break;
			}
		}
		if (!entry_point.empty() && model != spv::ExecutionModelMax)
			hlsl.set_entry_point(entry_point, model);

		// Compile to HLSL
		std::string source = "";
		try {
			hlsl.build_dummy_sampler_for_combined_images();
			hlsl.build_combined_image_samplers();
			source = hlsl.compile();
		} catch (spirv_cross::CompilerError& e) {
			ed::Logger::Get().Log("An exception occured: " + std::string(e.what()), true);
			return "error";
		}

		return source;
	}
	bool ShaderCompiler::CompileToSPIRV(std::vector<unsigned int>& spvOut, ShaderLanguage inLang, const std::string& filename, ShaderStage sType, const std::string& entry, std::vector<ShaderMacro>& macros, MessageStack* msgs, ProjectParser* project)
	{
		ed::Logger::Get().Log("Starting to transcompile a HLSL shader " + filename);

		std::string source;

		if (project != nullptr)
			source = project->LoadProjectFile(filename);
		else {
			// Load source into a string
			std::ifstream file(filename);

			if (!file.is_open()) {
				if (msgs != nullptr)
					msgs->Add(MessageStack::Type::Error, msgs->CurrentItem, "Failed to open file " + filename, -1, sType);
				file.close();
				return false;
			}
			source = std::string((std::istreambuf_iterator<char>(file)),
				std::istreambuf_iterator<char>());

			file.close();
		}

		return ShaderCompiler::CompileSourceToSPIRV(spvOut, inLang, filename, source, sType, entry, macros, msgs, project);
	}
	bool ShaderCompiler::CompileSourceToSPIRV(std::vector<unsigned int>& spvOut, ShaderLanguage inLang, const std::string& filename, const std::string& source, ShaderStage sType, const std::string& entry, std::vector<ShaderMacro>& macros, MessageStack* msgs, ProjectParser* project)
	{
		spvOut.clear();

		const char* inputStr = source.c_str();

		// create shader
		EShLanguage shaderType = EShLangVertex;
		if (sType == ShaderStage::Pixel)
			shaderType = EShLangFragment;
		else if (sType == ShaderStage::Geometry)
			shaderType = EShLangGeometry;
		else if (sType == ShaderStage::TessellationControl)
			shaderType = EShLangTessControl;
		else if (sType == ShaderStage::TessellationEvaluation)
			shaderType = EShLangTessEvaluation;
		else if (sType == ShaderStage::Compute)
			shaderType = EShLangCompute;

		glslang::TShader shader(shaderType);
		if (entry.size() > 0 && entry != "main") {
			shader.setEntryPoint(entry.c_str());
			shader.setSourceEntryPoint(entry.c_str());
		}
		shader.setStrings(&inputStr, 1);

		// set macros
		std::string preambleStr = (inLang == ShaderLanguage::HLSL) ? "#extension GL_GOOGLE_include_directive : enable\n" : "";
		if (inLang == ShaderLanguage::HLSL && (sType == ShaderStage::TessellationControl || sType == ShaderStage::TessellationEvaluation))
			preambleStr += "#extension GL_EXT_debug_printf : enable\n";

#ifdef SHADERED_WEB
		preambleStr += "#define SHADERED_WEB\n";
#else
		preambleStr += "#define SHADERED_DESKTOP\n";
#endif
		preambleStr += "#define SHADERED_VERSION " + std::to_string(SHADERED_VERSION) + "\n";

		for (auto& macro : macros) {
			if (!macro.Active)
				continue;
			preambleStr += "#define " + std::string(macro.Name) + " " + std::string(macro.Value) + "\n";
		}
		if (preambleStr.size() > 0)
			shader.setPreamble(preambleStr.c_str());

		glslang::EShClient gClient = glslang::EShClientOpenGL;
		// if (inLang == ShaderLanguage::VulkanGLSL)
		//	gClient = glslang::EShClientVulkan;

		// set up
		int sVersion = (sType == ShaderStage::Compute) ? 430 : 330;
		glslang::EShTargetClientVersion targetClientVersion = glslang::EShTargetOpenGL_450;
		glslang::EShTargetLanguageVersion targetLanguageVersion = glslang::EShTargetSpv_1_5;

		shader.setEnvInput(inLang == ShaderLanguage::HLSL ? glslang::EShSourceHlsl : glslang::EShSourceGlsl, shaderType, gClient, sVersion);
		shader.setEnvClient(gClient, targetClientVersion);
		shader.setEnvTarget(glslang::EShTargetSpv, targetLanguageVersion);

		TBuiltInResource res = DefaultTBuiltInResource;
		EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgKeepUncalled);

		const int defVersion = sVersion;

		// includer
		ShaderFileIncluder includer;
		includer.ProjectHandle = project;
		includer.pushExternalLocalDirectory(filename.substr(0, filename.find_last_of("/\\")));
		if (project != nullptr)
			for (auto& str : Settings::Instance().Project.IncludePaths)
				includer.pushExternalLocalDirectory(project->GetProjectPath(str));

		std::string processedShader;

		if (!shader.preprocess(&res, defVersion, ENoProfile, false, false, messages, &processedShader, includer)) {
			if (msgs != nullptr) {
				msgs->Add(gl::ParseGlslangMessages(msgs->CurrentItem, sType, shader.getInfoLog()));
				msgs->Add(MessageStack::Type::Error, msgs->CurrentItem, "Shader preprocessing failed", -1, sType);
			}
			return false;
		}

		// update strings
		const char* processedStr = processedShader.c_str();
		shader.setStrings(&processedStr, 1);

		// shader.setAutoMapBindings(true);
		shader.setAutoMapLocations(true);

		// parse
		if (!shader.parse(&res, 100, false, messages)) {
			if (msgs != nullptr)
				msgs->Add(gl::ParseGlslangMessages(msgs->CurrentItem, sType, shader.getInfoLog()));
			return false;
		}

		// link
		glslang::TProgram prog;
		prog.addShader(&shader);

		if (!prog.link(messages)) {
			if (msgs != nullptr) {
				msgs->Add(MessageStack::Type::Error, msgs->CurrentItem, "Shader linking failed", -1, sType);

				const char* infoLog = prog.getInfoLog();
				if (infoLog != nullptr) {
					std::string info(infoLog);
					size_t errorLoc = info.find("ERROR:");
					size_t errorEnd = info.find("\n", errorLoc + 1);
					if (errorLoc != std::string::npos && errorEnd != std::string::npos)
						msgs->Add(MessageStack::Type::Error, msgs->CurrentItem, info.substr(errorLoc + 7, errorEnd - errorLoc - 7), -1, sType);
				}
			}
			return false;
		}

		// convert to spirv
		spv::SpvBuildLogger logger;
		glslang::SpvOptions spvOptions;

		spvOptions.optimizeSize = false;
		spvOptions.disableOptimizer = true;
		spvOptions.generateDebugInfo = true;
		spvOptions.validate = true;

		glslang::GlslangToSpv(*prog.getIntermediate(shaderType), spvOut, &logger, &spvOptions);

		return true;
	}
	IPlugin1* ShaderCompiler::GetPluginLanguageFromExtension(int* lang, const std::string& filename, const std::vector<IPlugin1*>& pls)
	{
		std::string ext = filename.substr(filename.find_last_of('.') + 1);
		std::string langName = "";

		for (const auto& pair : Settings::Instance().General.PluginShaderExtensions)
			if (std::count(pair.second.begin(), pair.second.end(), ext) > 0) {
				langName = pair.first;
				break;
			}

		for (IPlugin1* pl : pls) {
			int langlen = pl->CustomLanguage_GetCount();
			for (int i = 0; i < langlen; i++) {
				if (langName == std::string(pl->CustomLanguage_GetName(i))) {
					*lang = i;
					return pl;
				}
			}
		}

		*lang = -1;
		return nullptr;
	}
	ShaderLanguage ShaderCompiler::GetShaderLanguageFromExtension(const std::string& file)
	{
		std::vector<std::string>& hlslExts = Settings::Instance().General.HLSLExtensions;
		std::vector<std::string>& vkExts = Settings::Instance().General.VulkanGLSLExtensions;
		std::string ext = file.substr(file.find_last_of('.') + 1);

		if (std::count(hlslExts.begin(), hlslExts.end(), ext) > 0)
			return ShaderLanguage::HLSL;
		if (std::count(vkExts.begin(), vkExts.end(), ext) > 0)
			return ShaderLanguage::VulkanGLSL;
		for (const auto& pair : Settings::Instance().General.PluginShaderExtensions)
			if (std::count(pair.second.begin(), pair.second.end(), ext) > 0)
				return ShaderLanguage::Plugin;

		return ShaderLanguage::GLSL;
	}

	bool ShaderCompiler::DisassembleSPIRV(spvgentwo::IReader& reader, spvgentwo::String& out, spvgentwo::HeapAllocator& alloc, bool useColorCodes)
	{
		spvgentwo::Module module(&alloc);
		spvgentwo::Grammar gram(&alloc);

		if (!module.read(reader, gram))
			return false;

		if (!module.resolveIDs())
			return false;

		if (!module.reconstructTypeAndConstantInfo())
			return false;

		if (!module.reconstructNames())
			return false;

		spvgentwo::ModulePrinter::ModuleStringPrinter printer(out, useColorCodes);
		spvgentwo::ModulePrinter::printModule(module, gram, printer);

		return true;
	}
	bool ShaderCompiler::DisassembleSPIRV(std::vector<unsigned int>& spv, std::string& out, bool useColorCodes)
	{
		BinaryVectorReader reader(spv);

		spvgentwo::HeapAllocator alloc;
		spvgentwo::String buffer(&alloc);

		bool ret = ShaderCompiler::DisassembleSPIRV(reader, buffer, alloc, useColorCodes);
		if (ret)
			out = std::string(buffer.c_str());
		return ret;
	}
	bool ShaderCompiler::DisassembleSPIRVFromFile(const std::string& filename, std::string& out, bool useColorCodes)
	{
		spvgentwo::HeapAllocator alloc;
		spvgentwo::BinaryFileReader reader(alloc, filename.c_str());
		spvgentwo::String buffer(&alloc);

		bool ret = ShaderCompiler::DisassembleSPIRV(reader, buffer, alloc, useColorCodes);
		if (ret)
			out = std::string(buffer.c_str());
		return ret;
	}
}
