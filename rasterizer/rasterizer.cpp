#include "rasterizer.h"

namespace rasterizer
{

Canvas* Rasterizer::canvas = nullptr;
CameraPtr Rasterizer::camera = nullptr;
LightPtr Rasterizer::light = nullptr;
ShaderBase* Rasterizer::shader = nullptr;
Matrix4x4 Rasterizer::transform;
RenderState Rasterizer::renderState;
RenderData Rasterizer::renderData;
VaryingDataBuffer Rasterizer::varyingDataBuffer;


bool Rasterizer::isDrawTextured = true;
bool Rasterizer::isDrawWireFrame = false;
bool Rasterizer::isDrawPoint = false;

int Rasterizer::pixelDrawCount = 0;
int Rasterizer::triangleDrawCount = 0;

void Rasterizer::Initialize()
{
    Texture::Initialize();
}

void Rasterizer::DrawLine(int x0, int x1, int y0, int y1, const Color32& color)
{
	bool steep = Mathf::Abs(y1 - y0) > Mathf::Abs(x1 - x0);
	if (steep)
	{
		std::swap(x0, y0);
		std::swap(x1, y1);
	}
	if (x0 > x1)
	{
		std::swap(x0, x1);
		std::swap(y0, y1);
	}

	int deltaX = x1 - x0;
	int deltaY = Mathf::Abs(y1 - y0);
	int error = deltaX / 2;
	int yStep = -1;
	if (y0 < y1) yStep = 1;

	for (int x = x0, y = y0; x < x1; ++x)
	{
		steep ? canvas->SetPixel(y, x, color) : canvas->SetPixel(x, y, color);
		error = error - deltaY;
		if (error < 0)
		{
			y = y + yStep;
			error = error + deltaX;
		}
	}
}

int Orient2D(int x0, int y0, int x1, int y1, int x2, int y2)
{
	return (x1 - x0) * (y2 - y1) - (y1 - y0) * (x2 - x1);
}

template<typename DrawDataType>
void Rasterizer::RasterizerTriangle(
	Triangle<Projection> projection, Render2x2Func<DrawDataType> renderFunc, const DrawDataType& renderData)
{
	const Projection& p0 = projection.v0;
	const Projection& p1 = projection.v1;
	const Projection& p2 = projection.v2;

	int minX = Mathf::Min(p0.x, p1.x, p2.x);
	int minY = Mathf::Min(p0.y, p1.y, p2.y);
	int maxX = Mathf::Max(p0.x, p1.x, p2.x);
	int maxY = Mathf::Max(p0.y, p1.y, p2.y);

	int width = canvas->GetWidth();
	int height = canvas->GetHeight();
	if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX >= width) maxX = width - 1;
	if (maxY >= height) maxY = height - 1;

	if (maxX < minX) return;
	if (maxY < minY) return;

	int dx01 = p0.x - p1.x;
	int dx12 = p1.x - p2.x;
	int dx20 = p2.x - p0.x;

	int dy01 = p0.y - p1.y;
	int dy12 = p1.y - p2.y;
	int dy20 = p2.y - p0.y;

	int startW0 = Orient2D(p0.x, p0.y, p1.x, p1.y, minX, minY);
	int startW1 = Orient2D(p1.x, p1.y, p2.x, p2.y, minX, minY);
	int startW2 = Orient2D(p2.x, p2.y, p0.x, p0.y, minX, minY);

	if (dy01 < 0 || (dy01 == 0 && dx01 < 0)) startW0 += 1;
	if (dy12 < 0 || (dy12 == 0 && dx12 < 0)) startW1 += 1;
	if (dy20 < 0 || (dy20 == 0 && dx20 < 0)) startW2 += 1;

	Rasterizer2x2Info info;
#if _MATH_SIMD_INTRINSIC_
	static __m128 _mf_one = _mm_set1_ps(1.f);

	__m128i mi_w0_delta = _mm_setr_epi32(0, dy01, -dx01, dy01 - dx01);
	__m128i mi_w1_delta = _mm_setr_epi32(0, dy12, -dx12, dy12 - dx12);
	__m128i mi_w2_delta = _mm_setr_epi32(0, dy20, -dx20, dy20 - dx20);

	__m128 mf_p0_invW = _mm_load1_ps(&(p0.invW));
	__m128 mf_p1_invW = _mm_load1_ps(&(p1.invW));
	__m128 mf_p2_invW = _mm_load1_ps(&(p2.invW));
#else
    int i_w0_delta[4] = {0, dy01, -dx01, dy01 - dx01};
    int i_w1_delta[4] = {0, dy12, -dx12, dy12 - dx12};
    int i_w2_delta[4] = {0, dy20, -dx20, dy20 - dx20};
#endif

	for (int y = minY; y <= maxY; y+=2)
	{
		int w0 = startW0;
		int w1 = startW1;
		int w2 = startW2;

		for (int x = minX; x <= maxX; x+=2)
		{
            info.maskCode = 0x00;
#if _MATH_SIMD_INTRINSIC_
			__m128i mi_w0 = _mm_add_epi32(_mm_set1_epi32(w0), mi_w0_delta);
			__m128i mi_w1 = _mm_add_epi32(_mm_set1_epi32(w1), mi_w1_delta);
			__m128i mi_w2 = _mm_add_epi32(_mm_set1_epi32(w2), mi_w2_delta);
            
            __m128i mi_or_w = _mm_or_si128(_mm_or_si128(mi_w0, mi_w1), mi_w2);
			if (_mm_extract_epi32(mi_or_w, 0) > 0) info.maskCode |= 0x1;
			if (_mm_extract_epi32(mi_or_w, 1) > 0) info.maskCode |= 0x2;
			if (_mm_extract_epi32(mi_or_w, 2) > 0) info.maskCode |= 0x4;
			if (_mm_extract_epi32(mi_or_w, 3) > 0) info.maskCode |= 0x8;
			if (info.maskCode != 0)
			{
				__m128 mf_w0 = _mm_cvtepi32_ps(mi_w0);
				__m128 mf_w1 = _mm_cvtepi32_ps(mi_w1);
				__m128 mf_w2 = _mm_cvtepi32_ps(mi_w2);

				__m128 mf_tmp0 = _mm_mul_ps(mf_w1, mf_p0_invW);
				__m128 mf_tmp1 = _mm_mul_ps(mf_w2, mf_p1_invW);
				__m128 mf_tmp2 = _mm_mul_ps(mf_w0, mf_p2_invW);
				//__m128 mf_invw = _mm_rcp_ps(_mm_add_ps(_mm_add_ps(mf_x, mf_y), mf_z));
				__m128 mf_invSum = _mm_div_ps(_mf_one, _mm_add_ps(_mm_add_ps(mf_tmp0, mf_tmp1), mf_tmp2));
				_mm_store_ps(info.wx, _mm_mul_ps(mf_tmp0, mf_invSum));
				_mm_store_ps(info.wy, _mm_mul_ps(mf_tmp1, mf_invSum));
				_mm_store_ps(info.wz, _mm_mul_ps(mf_tmp2, mf_invSum));

				__m128 mf_depth = _mm_mul_ps(_mm_add_ps(mf_w0, _mm_add_ps(mf_w1, mf_w2)), mf_invSum);
				_mm_store_ps(info.depth, mf_depth);
                
                for (int i = 0; i < 4; ++i)
                {
#else
            int i_w0[4], i_w1[4], i_w2[4];
            for (int i = 0; i < 4; ++i)
            {
                i_w0[i] = w0 + i_w0_delta[i];
                i_w1[i] = w1 + i_w1_delta[i];
                i_w2[i] = w2 + i_w2_delta[i];
                if ((i_w0[i] | i_w1[i] | i_w2[i]) > 0) info.maskCode |= (1<<i);
            }
                
			if (info.maskCode != 0)
            {
                for (int i=0; i<4; ++i)
                {
					float f_w0 = (float)i_w0[i];
					float f_w1 = (float)i_w1[i];
					float f_w2 = (float)i_w2[i];

					info.wx[i] = f_w1 * p0.invW;
					info.wy[i] = f_w2 * p1.invW;
					info.wz[i] = f_w0 * p2.invW;
					float f_invSum = 1.f / (info.wx[i] + info.wy[i] + info.wz[i]);
					info.wx[i] *= f_invSum;
					info.wy[i] *= f_invSum;
					info.wz[i] *= f_invSum;

					info.depth[i] = (f_w0 + f_w1 + f_w2) * f_invSum;
#endif
					info.depth[i] = camera->GetLinearDepth(info.depth[i]);
				}

				info.x = x;
				info.y = y;
				renderFunc(renderData, info);
			}


			w0 += dy01 * 2;
			w1 += dy12 * 2;
			w2 += dy20 * 2;
		}

		startW0 -= dx01 * 2;
		startW1 -= dx12 * 2;
		startW2 -= dx20 * 2;
	}
}

void Rasterizer::PrepareRender()
{
	pixelDrawCount = 0;
	triangleDrawCount = 0;
}

void Rasterizer::Render()
{
	/*
	int i = 0;
	int size = 0;

	size = (int)renderData.renderList[0].size();
//#pragma omp parallel for private(i)
	for (i = 0; i < size; ++i)
	{
		auto& pixel = renderData.renderList[0][i];
		if (pixel.depth <= canvas->GetDepth(pixel.x, pixel.y))
		{
			const auto& meshRef = renderData.meshes[pixel.meshIndex];
			const auto& triangleRef = meshRef.triangles[pixel.triangleIndex];

			PS ps;
			ps.material = triangleRef.material;
			ps.light = meshRef.light;
			ps.camera = meshRef.camera;
			ps.ddx = pixel.ddx;
			ps.ddy = pixel.ddy;

			Color color = ps.psMain(pixel.pixel);
			canvas->SetPixel(pixel.x, pixel.y, color);
			++pixelDrawCount;
		}
	}
    
    // transparent
	auto& renderList = renderData.renderList[1];
	std::vector<u32> indices(renderList.size(), 0);
	for (u32 i = 0; i < indices.size(); ++i) indices[i] = i;
	std::sort(indices.begin(), indices.end(), [&renderList](const u32& a, const u32& b)
	{
		return renderList[a].depth > renderList[b].depth;
	});

	size = (int)indices.size();
	for (i = 0; i < size; ++i)
	{
		u32 blockID = indices[i];
		auto& pixel = renderList[blockID];
		if (pixel.depth < canvas->GetDepth(pixel.x, pixel.y))
		{
			const auto& meshRef = renderData.meshes[pixel.meshIndex];
			const auto& triangleRef = meshRef.triangles[pixel.triangleIndex];

			PS ps;
			ps.material = triangleRef.material;
			ps.light = meshRef.light;
			ps.camera = meshRef.camera;
			ps.ddx = pixel.ddx;
			ps.ddy = pixel.ddy;
			Color color = ps.psMain(pixel.pixel);
			Color oc = canvas->GetPixel(pixel.x, pixel.y);
			color = Color::Lerp(color, oc, color.a);
			canvas->SetPixel(pixel.x, pixel.y, color);
		}
	}

	printf("Pixel Draw Count %d\n", pixelDrawCount);
	printf("Triangle Draw Count %d\n", triangleDrawCount);
	*/
}

void Rasterizer::Submit()
{
	assert(canvas != nullptr);
	assert(camera != nullptr);

	u32 width = canvas->GetWidth();
	u32 height = canvas->GetHeight();

	shader->_MATRIX_V = *camera->GetViewMatrix();
	shader->_MATRIX_P = *camera->GetProjectionMatrix();
	shader->_MATRIX_VP = shader->_MATRIX_P.Multiply(shader->_MATRIX_V);
	shader->_Object2World = transform;
	shader->_World2Object = transform.Inverse();
	shader->_MATRIX_MV = shader->_MATRIX_V.Multiply(transform);
	shader->_MATRIX_MVP = shader->_MATRIX_VP.Multiply(transform);

	shader->_WorldSpaceCameraPos = camera->GetPosition();

	u32 vertexCount = renderData.GetVertexCount();
	varyingDataBuffer.InitVerticesVaryingData(vertexCount);
	for (u32 i = 0; i < vertexCount; ++i)
	{
		shader->vertexVaryingData = &varyingDataBuffer.GetVertexVaryingData(i);
		void* vertexData = renderData.GetVertexData(i);
		shader->_VSMain(vertexData);
	}

	varyingDataBuffer.InitDynamicVaryingData();
	varyingDataBuffer.InitPixelVaryingData();

	int primitiveCount = renderData.GetPrimitiveCount();
	for (int i = 0; i < primitiveCount; ++i)
	{
		Triangle<u16> triangleIdx;
		if (!renderData.GetTrianglePrimitive(i, triangleIdx))
		{
			assert(false);
			continue;
		}

		auto v0 = varyingDataBuffer.GetVertexVaryingData(triangleIdx.v0);
		auto v1 = varyingDataBuffer.GetVertexVaryingData(triangleIdx.v1);
		auto v2 = varyingDataBuffer.GetVertexVaryingData(triangleIdx.v2);

		varyingDataBuffer.ResetDynamicVaryingData();
		
		auto triangles = Clipper::ClipTriangle(v0, v1, v2);
		for (auto& triangle : triangles)
		{
			Triangle<Projection> projection;
			projection.v0 = Projection::CalculateViewProjection(triangle.v0.position, width, height);
			projection.v1 = Projection::CalculateViewProjection(triangle.v1.position, width, height);
			projection.v2 = Projection::CalculateViewProjection(triangle.v2.position, width, height);

			int ret = Orient2D(projection.v0.x, projection.v0.y, projection.v1.x, projection.v1.y, projection.v2.x, projection.v2.y);
			switch (renderState.cull)
			{
			case RenderState::CullType_Front:
				if (ret >= 0) continue;
			case RenderState::CullType_Off:
				if (ret == 0) continue;
				if (ret < 0)
				{
					std::swap(projection.v1, projection.v2);
					std::swap(triangle.v1, triangle.v2);
				}
				break;
			case RenderState::CullType_Back:
				if (ret <= 0) continue;
				break;
			default:
				break;
			}

			varyingDataBuffer.ResetPixelVaryingData();
			RasterizerTriangle<decltype(triangle)>(projection, Rasterizer2x2RenderFunc, triangle);
		}
		

		/* draw wireframe
		std::vector<Line<VertexVaryingData> > lines;
		auto l1 = Clipper::ClipLine(v0, v1);
		lines.insert(lines.end(), l1.begin(), l1.end());
		auto l2 = Clipper::ClipLine(v0, v2);
		lines.insert(lines.end(), l2.begin(), l2.end());
		auto l3 = Clipper::ClipLine(v1, v2);
		lines.insert(lines.end(), l3.begin(), l3.end());

		for (auto& line : lines)
		{
			Projection p0 = Projection::CalculateViewProjection(line.v0.position, width, height);
			Projection p1 = Projection::CalculateViewProjection(line.v1.position, width, height);
			DrawLine(p0.x, p1.x, p0.y, p1.y, Color::blue);
		}
		*/
	}

	/* draw point
	for (u32 i = 0; i < vertexCount; ++i)
	{
		auto data = varyingDataBuffer.GetVertexVaryingData(i);
		if (data.clipCode != 0) continue;

		Projection proj = Projection::CalculateViewProjection(data.position, width, height);
		canvas->SetPixel(proj.x, proj.y, Color::red);
	}
	*/
}

void Rasterizer::SetShader(ShaderBase* shader)
{
	assert(shader != nullptr);
	Rasterizer::shader = shader;

	varyingDataBuffer.SetVaryingDataDecl(shader->varyingDataDecl, shader->varyingDataSize);
}

void Rasterizer::Rasterizer2x2RenderFunc(const Triangle<VertexVaryingData>& data, const Rasterizer2x2Info& quad)
{
	static int quadX[4] = { 0, 1, 0, 1 };
	static int quadY[4] = { 0, 0, 1, 1 };

	Quad<PixelVaryingData> pixelVaryingDataQuad;
	for (int i = 0; i < 4; ++i)
	{
		pixelVaryingDataQuad[i] = PixelVaryingData::TriangleInterp(data.v0, data.v1, data.v2, quad.wx[i], quad.wy[i], quad.wz[i]);
	}
	shader->_PassQuad(pixelVaryingDataQuad);

	for (int i = 0; i < 4; ++i)
	{
		if (!(quad.maskCode & (1 << i))) continue;

		int x = quad.x + quadX[i];
		int y = quad.y + quadY[i];

		if (!renderState.ZTest(quad.depth[i], canvas->GetDepth(x, y))) continue;
		if (renderState.zWrite) canvas->SetDepth(x, y, quad.depth[i]);

		shader->pixelVaryingData = &pixelVaryingDataQuad[i];
		Color color = shader->_PSMain();
		canvas->SetPixel(x, y, color);
	}
}

bool RenderState::ZTest(float zPixel, float zInBuffer)
{
	switch (ztest)
	{
	case rasterizer::RenderState::ZTestType_Always:
		return true;
	case rasterizer::RenderState::ZTestType_Less:
		return zPixel < zInBuffer;
	case rasterizer::RenderState::ZTestType_Greater:
		return zPixel > zInBuffer;
	case rasterizer::RenderState::ZTestType_LEqual:
		return zPixel <= zInBuffer;
	case rasterizer::RenderState::ZTestType_GEqual:
		return zPixel >= zInBuffer;
	case rasterizer::RenderState::ZTestType_Equal:
		return zPixel == zInBuffer;
	case rasterizer::RenderState::ZTestType_NotEqual:
		return zPixel != zInBuffer;
	default:
		break;
	}
	return false;
}

}