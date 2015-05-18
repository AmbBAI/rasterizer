#include "bitmap.h"
using namespace rasterizer;


BitmapPtr Bitmap::Create(u32 width, u32 height, BitmapType type)
{
	BitmapPtr bitmap = std::make_shared<Bitmap>();
	bitmap->width = width;
	bitmap->height = height;
	bitmap->type = type;
	if (bitmap->type == BitmapType_Unknown) return bitmap;

	u32 size = width * height;
	switch (type)
	{
	case Bitmap::BitmapType_L8:
		break;
	case Bitmap::BitmapType_RGB888:
		size *= 3;
		break;
	case Bitmap::BitmapType_RGBA8888:
		size <<= 2;
		break;
	case Bitmap::BitmapType_DXT1:
		if (width & 3) return nullptr;
		if (height & 3) return nullptr;
		size = (size >> 4) << 3;
		break;
	//case Bitmap::BitmapType_Normal:
	//	size <<= 1;
	//	break;
	}

	bitmap->bytes = new u8[size];
	return bitmap;
}

Bitmap::~Bitmap()
{
	if (bytes != nullptr)
	{
		delete bytes;
		bytes = nullptr;
	}
}

Color Bitmap::GetPixel_L8(u32 x, u32 y)
{
	u32 grey = (u32)*(bytes + (y * width + x));
	return Color32(grey, grey, grey, grey);
}

Color Bitmap::GetPixel_RGB888(u32 x, u32 y)
{
	u8* byte = bytes + (y * width + x) * 3;
	u8 r = *byte;
	u8 g = *(byte + 1);
	u8 b = *(byte + 2);
	return Color32(255, r, g, b);
}

Color Bitmap::GetPixel_RGBA8888(u32 x, u32 y)
{
	u8* byte = bytes + (y * width + x) * 4;
	u8 r = *byte;
	u8 g = *(byte + 1);
	u8 b = *(byte + 2);
	u8 a = *(byte + 3);
	return Color32(a, r, g, b);
}

Color Bitmap::GetPixel_DXT1(u32 x, u32 y)
{
	int offset = ((y >> 2) * (width >> 2) + (x >> 2)) << 3;
	u8* byte = bytes + offset;

	u32 indexBlock = *((u32*)byte + 1);
	u32 indexPos = ((((y & 3) << 2) | (x & 3)) << 1);
	u32 index = (indexBlock & (3u << indexPos)) >> indexPos;

	u32 colorByte = *((u32*)byte);
	Color c0, c1;
	if (index == 0 || (index & 2))
	{
		c0.a = 1.;
		c0.r = (float)((colorByte >> 27) & 31) / 31;
		c0.g = (float)((colorByte >> 21) & 63) / 63;
		c0.b = (float)((colorByte >> 16) & 31) / 31;
		if (index == 0) return c0;
	}
	
	if (index == 1 || (index & 2))
	{
		c1.a = 1.;
		c1.r = (float)((colorByte >> 11) & 31) / 31;
		c1.g = (float)((colorByte >> 5) & 63) / 63;
		c1.b = (float)((colorByte >> 0) & 31) / 31;
		if (index == 1) return c1;
	}

	if (index == 2)
	{
		return Color::Lerp(c0, c1, 0.33333333f);
	}
	else
	{
		return Color::Lerp(c0, c1, 0.66666667f);
	}
}

BitmapPtr Bitmap::CompressToDXT1()
{
	if (bytes == nullptr) return nullptr;
	if (type != Bitmap::BitmapType_RGB888) return nullptr;
	if (width < 4 || (width & 3) != 0) return nullptr;
	if (height < 4 || (height & 3) != 0) return nullptr;

	BitmapPtr bitmap = Create(width, height, Bitmap::BitmapType_DXT1);
	u8* newBytes = bitmap->GetBytes();
	for (u32 y = 0; y < height; y += 4)
	{
		for (u32 x = 0; x < width; x += 4)
		{
			Vector3 minColor(1.,1.,1.);
			Vector3 maxColor(0.,0.,0.);
			for (int i = 0; i < 16; ++i)
			{
				Color color = GetColor(x + (i & 3), y + (i >> 2));
				minColor.x = Mathf::Min(minColor.x, color.r);
				minColor.y = Mathf::Min(minColor.y, color.g);
				minColor.z = Mathf::Min(minColor.z, color.b);
				maxColor.x = Mathf::Max(maxColor.x, color.r);
				maxColor.y = Mathf::Max(maxColor.y, color.g);
				maxColor.z = Mathf::Max(maxColor.z, color.b);
			}

			Vector3 line(maxColor - minColor);
			float lineLen = line.Length();

			u32 indices = 0x0;
			if (Mathf::Approximately(lineLen, 0.f))
			{
				indices = (21845u << 16) | 21845u;
			}
			else
			{
				line /= lineLen;

				for (int i = 0; i < 16; ++i)
				{
					Color color = GetColor(x + (i & 3), y + (i >> 2));
					Vector3 sample(color.r, color.g, color.b);
					float iVal = (sample - minColor).Dot(line) / lineLen;
					u32 index = 0;
					if (iVal > 1.f / 6.f)
					{
						index += 2;
						if (iVal > 0.5f)
						{
							index += 1;
							if (iVal > 5.f / 6.f) index -= 2;
						}
					}
					indices |= (index << (i << 1));
				}
			}

			u32 color = 0x00;
			color |= (u32)(minColor.x * 31) << 27;
			color |= (u32)(minColor.y * 63) << 21;
			color |= (u32)(minColor.z * 31) << 16;
			color |= (u32)(maxColor.x * 31) << 11;
			color |= (u32)(maxColor.y * 63) << 5;
			color |= (u32)(maxColor.z * 31) << 0;

			int offset = ((y >> 2) * (width >> 2) + (x >> 2)) << 3;
			u8* byte = newBytes + offset;
			*((u32*)byte) = color;
			*((u32*)byte + 1) = indices;
		}
	}
	return bitmap;
}
