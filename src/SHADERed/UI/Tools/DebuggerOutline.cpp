#include <SHADERed/UI/Tools/DebuggerOutline.h>
#include <imgui/imgui.h>

namespace ed {
	glm::vec2 getScreenCoord(glm::vec4 v)
	{
		return glm::vec2(((v / v.w) + 1.0f) * 0.5f);
	}
	glm::vec2 getScreenCoordInverted(glm::vec4 v)
	{
		glm::vec2 ret = getScreenCoord(v);
		return glm::vec2(ret.x, 1 - ret.y);
	}

	void DebuggerOutline::RenderPrimitiveOutline(const PixelInformation& pixel, glm::vec2 uiPos, glm::vec2 itemSize, glm::vec2 zoomPos, glm::vec2 zoomSize)
	{
		constexpr unsigned int outlineColor = 0xffffffff;
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		glm::ivec2 vPos[6];
		int i, j;

		for (i = 0; i < pixel.VertexCount; ++i) {
			vPos[i] = (getScreenCoord(pixel.VertexShaderPosition[i]) - zoomPos) * (1.0f / zoomSize) * itemSize;
			vPos[i].y = static_cast<int>(itemSize.y) - vPos[i].y;
		}

		for (i = 0, j = 1; j < pixel.VertexCount; ++i, ++j)
			drawList->AddLine(ImVec2(uiPos.x + static_cast<float>(vPos[i].x), uiPos.y + static_cast<float>(vPos[i].y)), ImVec2(uiPos.x + static_cast<float>(vPos[j].x), uiPos.y + static_cast<float>(vPos[j].y)), outlineColor);
		if (pixel.VertexCount > 2)
			drawList->AddLine(ImVec2(uiPos.x + static_cast<float>(vPos[i].x), uiPos.y + static_cast<float>(vPos[i].y)), ImVec2(uiPos.x + static_cast<float>(vPos[0].x), uiPos.y + static_cast<float>(vPos[0].y)), outlineColor);

		for (i = 0; i < pixel.VertexCount; ++i) {
			char labelChar = static_cast<char>('0' + i);
			std::string label(1, labelChar);
			drawList->AddText(ImVec2(uiPos.x + static_cast<float>(vPos[i].x), uiPos.y + static_cast<float>(vPos[i].y)), outlineColor, label.c_str());
		}

		if (pixel.GeometryShaderUsed) {
			int gsOutputsCount = 0;
			glm::ivec2 gsPos[3];

			switch (pixel.GeometryOutputType) {
			case GeometryShaderOutput::Points:
				gsOutputsCount = 1;
				break;
			case GeometryShaderOutput::LineStrip:
				gsOutputsCount = 2;
				break;
			case GeometryShaderOutput::TriangleStrip:
				gsOutputsCount = 3;
				break;
			}

			for (i = 0; i < gsOutputsCount; ++i)
				gsPos[i] = (getScreenCoordInverted(pixel.FinalPosition[i]) - glm::vec2(zoomPos.x, -zoomPos.y)) * (1.0f / zoomSize) * itemSize;
			for (i = 0, j = 1; j < gsOutputsCount; ++i, ++j)
				drawList->AddLine(ImVec2(uiPos.x + static_cast<float>(gsPos[i].x), uiPos.y + static_cast<float>(gsPos[i].y)), ImVec2(uiPos.x + static_cast<float>(gsPos[j].x), uiPos.y + static_cast<float>(gsPos[j].y)), outlineColor);
			if (gsOutputsCount > 2)
				drawList->AddLine(ImVec2(uiPos.x + static_cast<float>(gsPos[i].x), uiPos.y + static_cast<float>(gsPos[i].y)), ImVec2(uiPos.x + static_cast<float>(gsPos[0].x), uiPos.y + static_cast<float>(gsPos[0].y)), outlineColor);
			for (i = 0; i < gsOutputsCount; ++i) {
				const char GS0[4] = { 'G', 'S', static_cast<char>('0' + i), '\0' };
				drawList->AddText(ImVec2(uiPos.x + static_cast<float>(gsPos[i].x), uiPos.y + static_cast<float>(gsPos[i].y)), outlineColor, GS0);
			}
		}
	}
	void DebuggerOutline::RenderPixelOutline(const PixelInformation& pixel, glm::vec2 uiPos, glm::vec2 itemSize, glm::vec2 zoomPos, glm::vec2 zoomSize)
	{
		constexpr unsigned int outlineColor = 0xffffffff;
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		glm::vec2 relCoord = glm::vec2(pixel.Coordinate) / glm::vec2(pixel.RenderTextureSize);
		relCoord += glm::vec2(0.5f / static_cast<float>(pixel.RenderTextureSize.x), 0.5f / static_cast<float>(pixel.RenderTextureSize.y));

		glm::ivec2 pxPos = (relCoord - zoomPos) * (1.0f / zoomSize) * itemSize;
		pxPos.y = static_cast<int>(itemSize.y) - pxPos.y;

		float hfpxSizeX = 0.5f / zoomSize.x;
		float hfpxSizeY = 0.5f / zoomSize.y;

		// lines
		drawList->AddLine(ImVec2(uiPos.x, uiPos.y + static_cast<float>(pxPos.y)), ImVec2(uiPos.x + static_cast<float>(pxPos.x) - hfpxSizeY, uiPos.y + static_cast<float>(pxPos.y)), outlineColor);
		drawList->AddLine(ImVec2(uiPos.x + static_cast<float>(pxPos.x) + hfpxSizeY, uiPos.y + static_cast<float>(pxPos.y)), ImVec2(uiPos.x + itemSize.x, uiPos.y + static_cast<float>(pxPos.y)), outlineColor);
		drawList->AddLine(ImVec2(uiPos.x + static_cast<float>(pxPos.x), uiPos.y), ImVec2(uiPos.x + static_cast<float>(pxPos.x), uiPos.y + static_cast<float>(pxPos.y) - hfpxSizeY), outlineColor);
		drawList->AddLine(ImVec2(uiPos.x + static_cast<float>(pxPos.x), uiPos.y + static_cast<float>(pxPos.y) + hfpxSizeY), ImVec2(uiPos.x + static_cast<float>(pxPos.x), uiPos.y + itemSize.y), outlineColor);

		// rectangle
		drawList->AddLine(ImVec2(uiPos.x + static_cast<float>(pxPos.x) - hfpxSizeX, uiPos.y + static_cast<float>(pxPos.y) - hfpxSizeY), ImVec2(uiPos.x + static_cast<float>(pxPos.x) - hfpxSizeX, uiPos.y + static_cast<float>(pxPos.y) + hfpxSizeY), outlineColor);
		drawList->AddLine(ImVec2(uiPos.x + static_cast<float>(pxPos.x) + hfpxSizeX, uiPos.y + static_cast<float>(pxPos.y) - hfpxSizeY), ImVec2(uiPos.x + static_cast<float>(pxPos.x) + hfpxSizeX, uiPos.y + static_cast<float>(pxPos.y) + hfpxSizeY), outlineColor);
		drawList->AddLine(ImVec2(uiPos.x + static_cast<float>(pxPos.x) - hfpxSizeX, uiPos.y + static_cast<float>(pxPos.y) + hfpxSizeY), ImVec2(uiPos.x + static_cast<float>(pxPos.x) + hfpxSizeX, uiPos.y + static_cast<float>(pxPos.y) + hfpxSizeY), outlineColor);
		drawList->AddLine(ImVec2(uiPos.x + static_cast<float>(pxPos.x) - hfpxSizeX, uiPos.y + static_cast<float>(pxPos.y) - hfpxSizeY), ImVec2(uiPos.x + static_cast<float>(pxPos.x) + hfpxSizeX, uiPos.y + static_cast<float>(pxPos.y) - hfpxSizeY), outlineColor);

		if (zoomSize.x < 0.3f) {
			static char pxCoord[32] = { 0 };
			snprintf(pxCoord, sizeof(pxCoord), "(%d, %d)", pixel.Coordinate.x, pixel.Coordinate.y);
			drawList->AddText(ImVec2(uiPos.x + static_cast<float>(pxPos.x) + hfpxSizeX, uiPos.y + static_cast<float>(pxPos.y) - hfpxSizeY), outlineColor, pxCoord);
		}
	}
}