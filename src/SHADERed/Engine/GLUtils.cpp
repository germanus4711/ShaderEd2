#include <SHADERed/Engine/GLUtils.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/ObjectManager.h>
#include <sstream>
#include <string>

#include <glm/gtc/type_ptr.hpp>

namespace ed::gl {
	GLuint CreateSimpleFramebuffer(GLint width, GLint height, GLuint& texColor, GLuint& texDepth, GLuint fmt)
	{
		// create a texture for color information
		glGenTextures(1, &texColor);
		glBindTexture(GL_TEXTURE_2D, texColor);
		glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(fmt), width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, 0);

		glGenTextures(1, &texDepth);
		glBindTexture(GL_TEXTURE_2D, texDepth);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);

		GLuint ret, actualRet = 1;
		glGenFramebuffers(1, &ret);
		glBindFramebuffer(GL_FRAMEBUFFER, ret);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texColor, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, texDepth, 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			actualRet = 0;

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		return actualRet * ret;
	}
	void FreeSimpleFramebuffer(const GLuint& fbo, const GLuint& color, const GLuint& depth)
	{
		glDeleteTextures(1, &color);
		glDeleteTextures(1, &depth);
		glDeleteFramebuffers(1, &fbo);
	}
	GLuint CreateShader(const char** vsCode, const char** psCode, const std::string& name)
	{
		GLint success = 0;
		char infoLog[512];

		// create vertex shader
		unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vs, 1, vsCode, nullptr);
		glCompileShader(vs);
		glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(vs, 512, nullptr, infoLog);
			ed::Logger::Get().Log("Failed to compile " + name + " vertex shader", true);
			ed::Logger::Get().Log(infoLog, true);
		}

		// create pixel shader
		unsigned int ps = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(ps, 1, psCode, nullptr);
		glCompileShader(ps);
		glGetShaderiv(ps, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(ps, 512, nullptr, infoLog);
			ed::Logger::Get().Log("Failed to compile " + name + " pixel shader", true);
			ed::Logger::Get().Log(infoLog, true);
		}

		// create a shader program for cubemap preview
		GLuint shader = glCreateProgram();
		glAttachShader(shader, vs);
		glAttachShader(shader, ps);
		glLinkProgram(shader);
		glGetProgramiv(shader, GL_LINK_STATUS, &success);
		if (!success) {
			glGetProgramInfoLog(shader, 512, nullptr, infoLog);
			ed::Logger::Get().Log("Failed to create a " + name + " shader program", true);
			ed::Logger::Get().Log(infoLog, true);
		}

		glDeleteShader(vs);
		glDeleteShader(ps);

		return shader;
	}
	GLuint CompileShader(const GLenum type, const GLchar* str)
	{
		// create pixel shader
		const GLuint ret = glCreateShader(type);

		// compile pixel shader
		glShaderSource(ret, 1, &str, nullptr);
		glCompileShader(ret);

		return ret;
	}
	bool CheckShaderCompilationStatus(const GLuint shader, GLchar* msg)
	{
		GLint ret = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &ret);
		if (!ret && msg)
			glGetShaderInfoLog(shader, 1024, nullptr, msg);
		return static_cast<bool>(ret);
	}
	bool CheckShaderLinkStatus(GLuint shader, GLchar* msg)
	{
		GLint ret = 0;
		glGetProgramiv(shader, GL_LINK_STATUS, &ret);
		if (!ret && msg)
			glGetProgramInfoLog(shader, 1024, nullptr, msg);
		return static_cast<bool>(ret);
	}

	void CreateBufferVAO(GLuint& geoVAO, const GLuint geoVBO, const std::vector<ed::ShaderVariable::ValueType>& ilayout,
		const GLuint bufVBO, const std::vector<ed::ShaderVariable::ValueType>& types)
	{
		int fmtIndex = 0;

		if (geoVAO != 0)
			glDeleteVertexArrays(1, &geoVAO);
		glGenVertexArrays(1, &geoVAO);

		glBindVertexArray(geoVAO);
		glBindBuffer(GL_ARRAY_BUFFER, geoVBO);

		int rowStride = 0, curStride = 0;
		for (const auto& layitem : ilayout)
			rowStride += ShaderVariable::GetSize(layitem, true);

		for (const auto& layitem : ilayout) {
			// vertex positions
			glVertexAttribPointer(fmtIndex, ShaderVariable::GetSize(layitem) / 4, GL_FLOAT, GL_FALSE, rowStride,
				reinterpret_cast<void*>(static_cast<intptr_t>(curStride)));
			glEnableVertexAttribArray(fmtIndex);
			fmtIndex++;
			curStride += ShaderVariable::GetSize(layitem, true);
		}

		// user defined
		if (bufVBO != 0) {
			int sizeInBytes = 0;
			for (const auto& fmt : types)
				sizeInBytes += ed::ShaderVariable::GetSize(fmt);

			glBindBuffer(GL_ARRAY_BUFFER, bufVBO);
			int fmtOffset = 0;
			for (const auto& fmt : types) {
				GLint colCount = 0;
				GLenum type = GL_FLOAT;

				// clang-format off
					switch (fmt) {
						case ShaderVariable::ValueType::Boolean1: colCount = 1; type = GL_BYTE; break;
						case ShaderVariable::ValueType::Boolean2: colCount = 2; type = GL_BYTE; break;
						case ShaderVariable::ValueType::Boolean3: colCount = 3; type = GL_BYTE; break;
						case ShaderVariable::ValueType::Boolean4: colCount = 4; type = GL_BYTE; break;
						case ShaderVariable::ValueType::Integer1: colCount = 1; type = GL_INT; break;
						case ShaderVariable::ValueType::Integer2: colCount = 2; type = GL_INT; break;
						case ShaderVariable::ValueType::Integer3: colCount = 3; type = GL_INT; break;
						case ShaderVariable::ValueType::Integer4: colCount = 4; type = GL_INT; break;
						case ShaderVariable::ValueType::Float1: colCount = 1; type = GL_FLOAT; break;
						case ShaderVariable::ValueType::Float2: colCount = 2; type = GL_FLOAT; break;
						case ShaderVariable::ValueType::Float3: colCount = 3; type = GL_FLOAT; break;
						case ShaderVariable::ValueType::Float4: colCount = 4; type = GL_FLOAT; break;
						case ShaderVariable::ValueType::Float2x2: colCount = 2; type = GL_FLOAT; break;
						case ShaderVariable::ValueType::Float3x3: colCount = 3; type = GL_FLOAT; break;
						case ShaderVariable::ValueType::Float4x4: colCount = 4; type = GL_FLOAT; break;default: ;
					}
				// clang-format on

				glVertexAttribPointer(fmtIndex, colCount, type, GL_FALSE, sizeInBytes,
					reinterpret_cast<void*>(static_cast<intptr_t>(fmtOffset)));
				glEnableVertexAttribArray(fmtIndex);
				glVertexAttribDivisor(fmtIndex, 1);

				fmtOffset += ShaderVariable::GetSize(fmt);
				fmtIndex++;
			}
		}

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void CreateVAO(GLuint& geoVAO, const GLuint geoVBO, const std::vector<InputLayoutItem>& ilayout, const GLuint geoEBO, const GLuint bufVBO, const std::vector<ed::ShaderVariable::ValueType>& types)
	{
		int fmtIndex = 0;

		if (geoVAO != 0)
			glDeleteVertexArrays(1, &geoVAO);
		glGenVertexArrays(1, &geoVAO);

		glBindVertexArray(geoVAO);
		glBindBuffer(GL_ARRAY_BUFFER, geoVBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geoEBO);

		GLuint layOffset = 0;
		for (const auto& layitem : ilayout) {
			const auto size = static_cast<GLint>(InputLayoutItem::GetValueSize(layitem.Value));
			auto offset = static_cast<GLint>(InputLayoutItem::GetValueOffset(layitem.Value) * sizeof(GLfloat));
			if (layitem.Value >= InputLayoutValue::BufferFloat && layitem.Value <= InputLayoutValue::BufferInt4)
				offset = static_cast<GLint>(layOffset);

			// vertex positions
			glVertexAttribPointer(fmtIndex, size, GL_FLOAT, GL_FALSE, 18 * sizeof(float),
				reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
			glEnableVertexAttribArray(fmtIndex);
			fmtIndex++;
			layOffset += size;
		}

		// user defined
		if (bufVBO != 0) {
			int sizeInBytes = 0;
			for (const auto& fmt : types)
				sizeInBytes += ed::ShaderVariable::GetSize(fmt);

			glBindBuffer(GL_ARRAY_BUFFER, bufVBO);
			int fmtOffset = 0;
			for (const auto& fmt : types) {
				GLint colCount = 0;
				GLenum type = GL_FLOAT;

				// clang-format off
					switch (fmt) {
						case ShaderVariable::ValueType::Boolean1: colCount = 1; type = GL_BYTE; break;
						case ShaderVariable::ValueType::Boolean2: colCount = 2; type = GL_BYTE; break;
						case ShaderVariable::ValueType::Boolean3: colCount = 3; type = GL_BYTE; break;
						case ShaderVariable::ValueType::Boolean4: colCount = 4; type = GL_BYTE; break;
						case ShaderVariable::ValueType::Integer1: colCount = 1; type = GL_INT; break;
						case ShaderVariable::ValueType::Integer2: colCount = 2; type = GL_INT; break;
						case ShaderVariable::ValueType::Integer3: colCount = 3; type = GL_INT; break;
						case ShaderVariable::ValueType::Integer4: colCount = 4; type = GL_INT; break;
						case ShaderVariable::ValueType::Float1: colCount = 1; type = GL_FLOAT; break;
						case ShaderVariable::ValueType::Float2: colCount = 2; type = GL_FLOAT; break;
						case ShaderVariable::ValueType::Float3: colCount = 3; type = GL_FLOAT; break;
						case ShaderVariable::ValueType::Float4: colCount = 4; type = GL_FLOAT; break;
						case ShaderVariable::ValueType::Float2x2: colCount = 2; type = GL_FLOAT; break;
						case ShaderVariable::ValueType::Float3x3: colCount = 3; type = GL_FLOAT; break;
						case ShaderVariable::ValueType::Float4x4: colCount = 4; type = GL_FLOAT; break;case ShaderVariable::ValueType::Count: break;
					}
				// clang-format on

				glVertexAttribPointer(fmtIndex, colCount, type, GL_FALSE, sizeInBytes,
					reinterpret_cast<void*>(static_cast<intptr_t>(fmtOffset)));
				glEnableVertexAttribArray(fmtIndex);
				glVertexAttribDivisor(fmtIndex, 1);

				fmtOffset += ShaderVariable::GetSize(fmt);
				fmtIndex++;
			}
		}

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	std::vector<InputLayoutItem> CreateDefaultInputLayout()
	{
		std::vector<InputLayoutItem> ret;
		ret.push_back({ InputLayoutValue::Position, "POSITION" });
		ret.push_back({ InputLayoutValue::Normal, "NORMAL" });
		ret.push_back({ InputLayoutValue::Texcoord, "TEXCOORD0" });
		return ret;
	}

	void GetVertexBufferBounds(ObjectManager* objs, pipe::VertexBuffer* model, glm::vec3& minPosItem, glm::vec3& maxPosItem)
	{
		auto* buffer = static_cast<BufferObject*>(model->Buffer);

		if (buffer == nullptr) {
			minPosItem = glm::vec3(0.0f);
			maxPosItem = glm::vec3(0.0f);
			return;
		}

		const std::vector<ShaderVariable::ValueType> tData = objs->ParseBufferFormat(buffer->ViewFormat);

		int stride = 0;
		for (const auto& dataEl : tData)
			stride += ShaderVariable::GetSize(dataEl, true);

		auto* bufPtr = static_cast<GLfloat*>(malloc(buffer->Size));

		glBindBuffer(GL_ARRAY_BUFFER, buffer->ID);
		glGetBufferSubData(GL_ARRAY_BUFFER, 0, buffer->Size, &bufPtr[0]);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		const int rows = buffer->Size / stride;

		if (!tData.empty()) {
			minPosItem = glm::vec3(0.0f);
			maxPosItem = glm::vec3(0.0f);

			int rowStride = stride / 4;

			for (int r = 0; r < rows; r++) {
				const GLfloat* curPtr = bufPtr + r * rowStride;

				int elCount = ShaderVariable::GetSize(tData[0]) / 4;
				if (elCount >= 4) elCount = 3;

				for (int c = 0; c < elCount; c++) {
					minPosItem[c] = glm::min(minPosItem[c], curPtr[c]);
					maxPosItem[c] = glm::max(maxPosItem[c], curPtr[c]);
				}
			}
		}

		free(bufPtr);
	}

	bool isAllDigits(const std::string_view str)
	{
		return std::all_of(str.begin(), str.end(), [](const char c) { return std::isdigit(c); });
	}

	std::vector<MessageStack::Message> ParseGlslangMessages(const std::string& owner, ShaderStage stage, const std::string& str)
	{
		std::vector<MessageStack::Message> ret;

		std::istringstream f(str);
		std::string line;
		while (std::getline(f, line)) {
			if (line.find("ERROR:") != std::string::npos) {
				size_t firstD = line.find_first_of(':');
				size_t secondD = line.find_first_of(':', firstD + 1);
				size_t thirdD = line.find_first_of(':', secondD + 1);

				if (firstD == std::string::npos || secondD == std::string::npos || thirdD == std::string::npos)
					continue;

				int lineNr = -1;
				auto lineStr = line.substr(secondD + 1, thirdD - (secondD + 1));
				if (isAllDigits(lineStr))
					lineNr = std::stoi(lineStr);
				auto msg = line.substr(thirdD + 2);
				ret.emplace_back(MessageStack::Type::Error, owner, msg, lineNr, stage);
			} else if (!line.empty() && line[0] == '(' && line.find("error") != std::string::npos) {
				size_t firstP = line.find_first_of(')');

				int lineNr = -1;
				if (auto lineStr = line.substr(1, firstP - 1); isAllDigits(lineStr))
					lineNr = std::stoi(lineStr);

				if (line.size() > firstP + 3) {
					std::string msg = line.substr(firstP + 3);
					ret.emplace_back(MessageStack::Type::Error, owner, msg, lineNr, stage);
				}
			}
		}

		return ret;
	}
}
