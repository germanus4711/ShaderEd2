#include <SHADERed/Objects/InputLayout.h>

namespace ed {
	size_t InputLayoutItem::GetValueSize(InputLayoutValue val)
	{
		switch (val) {
		case InputLayoutValue::Position: return 3;
		case InputLayoutValue::Normal: return 3;
		case InputLayoutValue::Texcoord: return 2;
		case InputLayoutValue::Tangent: return 3;
		case InputLayoutValue::Binormal: return 3;
		case InputLayoutValue::Color: return 4;
		case InputLayoutValue::BufferFloat: return 1;
		case InputLayoutValue::BufferFloat2: return 2;
		case InputLayoutValue::BufferFloat3: return 3;
		case InputLayoutValue::BufferFloat4: return 4;
		case InputLayoutValue::BufferInt: return 1;
		case InputLayoutValue::BufferInt2: return 2;
		case InputLayoutValue::BufferInt3: return 3;
		case InputLayoutValue::BufferInt4: return 4;
		case InputLayoutValue::MaxCount: break;
		}
		return 0;
	}
	size_t InputLayoutItem::GetValueOffset(InputLayoutValue val)
	{
		switch (val) {
		case InputLayoutValue::Position: return 0;
		case InputLayoutValue::Normal: return 3;
		case InputLayoutValue::Texcoord: return 6;
		case InputLayoutValue::Tangent: return 8;
		case InputLayoutValue::Binormal: return 11;
		case InputLayoutValue::Color: return 14;
		case InputLayoutValue::BufferFloat: break;
		case InputLayoutValue::BufferFloat2: break;
		case InputLayoutValue::BufferFloat3: break;
		case InputLayoutValue::BufferFloat4: break;
		case InputLayoutValue::BufferInt: break;
		case InputLayoutValue::BufferInt2: break;
		case InputLayoutValue::BufferInt3: break;
		case InputLayoutValue::BufferInt4: break;
		case InputLayoutValue::MaxCount: break;
		}
		return 0;
	}
}