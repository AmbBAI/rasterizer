#include "rasterizer.h"

namespace rasterizer
{

Canvas* Rasterizer::canvas = nullptr;
CameraPtr Rasterizer::camera;
rasterizer::MaterialPtr Rasterizer::material;
rasterizer::Vector3 Rasterizer::lightDir = Vector3(-1.f, -1.f, -1.f).Normalize();
rasterizer::Shader0 Rasterizer::shader;


void Rasterizer::Initialize()
{
    Texture::Initialize();
}
    
void TestColor(Canvas* canvas)
{
	int w = canvas->GetWidth();
	int h = canvas->GetHeight();

	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			canvas->SetPixel(x, y,
				Color(1.f, (float)x / w, (float)y / h, 0.f));
		}
	}
}

void TestTexture(Canvas* canvas, const Vector4& rect, const Texture& texture, int miplv /*= 0*/)
{
	int width = (int)canvas->GetWidth();
	int height = (int)canvas->GetHeight();

	int minX = Mathf::Clamp(Mathf::FloorToInt(rect.x), 0, width);
	int maxX = Mathf::Clamp(Mathf::FloorToInt(rect.z), 0, width);
	int minY = Mathf::Clamp(Mathf::CeilToInt(rect.y), 0, height);
	int maxY = Mathf::Clamp(Mathf::CeilToInt(rect.w), 0, height);

	for (int y = minY; y < maxY; ++y)
	{
		float v = ((float)y - rect.y) / (rect.w - rect.y);
		for (int x = minX; x < maxX; ++x)
		{
			float u = ((float)x - rect.x) / (rect.z - rect.x);
			canvas->SetPixel(x, y, texture.Sample(u, v, miplv));
		}
	}
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

void Rasterizer::DrawSmoothLine(float x0, float x1, float y0, float y1, const Color32& color)
{
	float deltaX = x1 - x0;
	float deltaY = y1 - y0;

	bool steep = Mathf::Abs(deltaX) < Mathf::Abs(deltaY);
	if (steep)
	{
		std::swap(x0, y0);
		std::swap(x1, y1);
		std::swap(deltaX, deltaY);
	}
	if (x1 < x0)
	{
		std::swap(x0, x1);
		std::swap(y0, y1);
	}

	float gradient = (float)deltaY / deltaX;

	float xend, yend, xgap, intery;
	int xpxl1, xpxl2, ypxl1, ypxl2;

	float yfpart;

	xend = Mathf::Round(x0);
	yend = y0 + gradient * (xend - x0);
	xgap = 1.f - Mathf::Fractional(x0 + 0.5f);
	xpxl1 = Mathf::RoundToInt(xend);
	ypxl1 = Mathf::TruncToInt(yend);
	yfpart = Mathf::Fractional(yend);
	Plot(xpxl1, ypxl1, color, xgap * (1 - yfpart), steep);
	Plot(xpxl1, ypxl1 + 1, color, xgap * yfpart, steep);
	intery = yend + gradient;

	xend = Mathf::Round(x1);
	yend = y1 + gradient * (xend - x1);
	xgap = 1.f - Mathf::Fractional(x1 + 0.5f);
	xpxl2 = Mathf::RoundToInt(xend);
	ypxl2 = Mathf::TruncToInt(yend);
	yfpart = Mathf::Fractional(yend);
	Plot(xpxl2, ypxl2, color, xgap * (1 - yfpart), steep);
	Plot(xpxl2, ypxl2 + 1, color, xgap * yfpart, steep);

	for (int x = xpxl1 + 1; x <= xpxl2 - 1; ++x)
	{
		int ipartIntery = Mathf::TruncToInt(intery);
		float fpartIntery = Mathf::Fractional(intery);
		Plot(x, ipartIntery, color, 1.0f - fpartIntery, steep);
		Plot(x, ipartIntery + 1, color, fpartIntery, steep);

		intery += gradient;
	}

}

void Rasterizer::Plot(int x, int y, const Color32& color, float alpha, bool swapXY /*= false*/)
{
	assert(canvas != nullptr);

	Color32 drawColor = color;
	drawColor.a = (u8)(drawColor.a * Mathf::Clamp01(alpha));
	swapXY ? canvas->SetPixel(y, x, color) : canvas->SetPixel(x, y, color);
}

void Rasterizer::DrawMeshPoint(const Mesh& mesh, const Matrix4x4& transform, const Color32& color)
{
    assert(canvas != nullptr);
    assert(camera != nullptr);
    
	u32 width = canvas->GetWidth();
	u32 height = canvas->GetHeight();

    const Matrix4x4* view = camera->GetViewMatrix();
    const Matrix4x4* projection = camera->GetProjectionMatrix();
    Matrix4x4 mvp = (*projection).Multiply((*view).Multiply(transform));
    
    int vertexN = (int)mesh.vertices.size();
    
    int i = 0;
//#pragma omp parallel for private(i)
    for (i = 0; i < vertexN; ++i)
    {
        Vector4 position = mvp.MultiplyPoint(mesh.vertices[i]);
        u32 clipCode = Clipper::CalculateClipCode(position);
        if (0 != clipCode) continue;
        
		Projection point = Projection::CalculateViewProjection(position, width, height);
        canvas->SetPixel(point.x, point.y, color);
    }
}

void Rasterizer::DrawMeshWireFrame(const Mesh& mesh, const Matrix4x4& transform, const Color32& color)
{
	assert(canvas != nullptr);
	assert(camera != nullptr);

	u32 width = canvas->GetWidth();
	u32 height = canvas->GetHeight();

    const Matrix4x4* view = camera->GetViewMatrix();
    const Matrix4x4* projection = camera->GetProjectionMatrix();
    Matrix4x4 mvp = (*projection).Multiply((*view).Multiply(transform));

    int vertexN = (int)mesh.vertices.size();
    
	std::vector<VertexBase> vertices;
	vertices.resize(vertexN);
	for (int i = 0; i < (int)mesh.vertices.size(); ++i)
	{
		VertexBase vertex;
        vertex.position = mvp.MultiplyPoint(mesh.vertices[i]);
		vertex.clipCode = Clipper::CalculateClipCode(vertex.position);
		vertices[i] = vertex;
	}

	for (int i = 0; i + 2 < (int)mesh.indices.size(); i += 3)
	{
		int v0 = mesh.indices[i];
		int v1 = mesh.indices[i + 1];
		int v2 = mesh.indices[i + 2];
        
        //if (IsBackFace(vertices[v0].position, vertices[v1].position, vertices[v2].position)) continue;

		std::vector<Line<VertexBase> > lines;
        auto l1 = Clipper::ClipLine(vertices[v0], vertices[v1]);
        lines.insert(lines.end(), l1.begin(), l1.end());
		auto l2 = Clipper::ClipLine(vertices[v0], vertices[v2]);
        lines.insert(lines.end(), l2.begin(), l2.end());
		auto l3 = Clipper::ClipLine(vertices[v1], vertices[v2]);
        lines.insert(lines.end(), l3.begin(), l3.end());
        
        for (auto line : lines)
        {
			Projection p0 = line.v0.GetViewProjection(width, height);
			Projection p1 = line.v1.GetViewProjection(width, height);
			DrawLine(p0.x, p1.x, p0.y, p1.y, color);
        }
	}
}

int Rasterizer::Orient2D(int x0, int y0, int x1, int y1, int x2, int y2)
{
	return (x1 - x0) * (y2 - y1) - (y1 - y0) * (x2 - x1);
}

void Rasterizer::DrawTriangle(const Projection& p0, const Projection& p1, const Projection& p2, const Triangle<VertexStd>& triangle)
{
	//TODO 2x2 for mipmap level
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

	for (int y = minY; y <= maxY; ++y)
	{
		int w0 = startW0;
		int w1 = startW1;
		int w2 = startW2;

		for (int x = minX; x <= maxX; ++x)
		{
			if (w0 > 0 && w1 > 0 && w2 > 0)
			{
                Vector4 interp;
				interp.x = w1 * p0.invW;
				interp.y = w2 * p1.invW;
				interp.z = w0 * p2.invW;
				interp.w = 1.0f / (interp.x + interp.y + interp.z);

				float depth = (w0 + w1 + w2) / (w1 * p0.invZ + w2 * p1.invZ + w0 * p2.invZ);
                if (depth > 0.f && depth < 1.f && depth < canvas->GetDepth(x, y))
				{
					canvas->SetDepth(x, y, depth);
					//canvas->SetPixel(x, y, Color(1.f, 1.f - depth, 1.f - depth, 1.f - depth));
					
					PSInput psInput(triangle.v0, triangle.v1, triangle.v2, interp);
					Color color = shader.PixelShader(psInput);
					canvas->SetPixel(x, y, color);
				}
			}

			w0 += dy01;
			w1 += dy12;
			w2 += dy20;
		}

		startW0 -= dx01;
		startW1 -= dx12;
		startW2 -= dx20;
	}
}

void Rasterizer::DrawMesh(const Mesh& mesh, const Matrix4x4& transform)
{
	assert(canvas != nullptr);
	assert(camera != nullptr);

	const Matrix4x4* view = camera->GetViewMatrix();
	const Matrix4x4* projection = camera->GetProjectionMatrix();
    Matrix4x4 mvp = (*projection).Multiply((*view).Multiply(transform));
	shader._MATRIX_VIEW = const_cast<Matrix4x4*>(view);
	shader._MATRIX_PROJECTION = const_cast<Matrix4x4*>(projection);
	shader._MATRIX_MVP = &mvp;
	shader._MATRIX_OBJ_TO_WORLD = const_cast<Matrix4x4*>(&transform);
	shader.material = Rasterizer::material;
	shader.lightDir = Rasterizer::lightDir;

	u32 width = canvas->GetWidth();
	u32 height = canvas->GetHeight();

	int vertexN = (int)mesh.vertices.size();
	std::vector<VertexStd> vertices;
	vertices.resize(vertexN);

	int i = 0;
//#pragma omp parallel for private(i)
	for (i = 0; i < vertexN; ++i)
	{
		shader.position = const_cast<Vector3*>(&mesh.vertices[i]);
		shader.normal = const_cast<Vector3*>(&mesh.normals[i]);
		shader.tangent = const_cast<Vector3*>(&mesh.tangents[i]);
		shader.texcoord = const_cast<Vector2*>(&mesh.texcoords[i]);
		shader.VertexShader(vertices[i]);
	}

	int faceN = (int)mesh.indices.size() / 3;
//#pragma omp parallel for private(i)
	for (i = 0; i < faceN; ++i)
	{
		int i0 = mesh.indices[i * 3];
		int i1 = mesh.indices[i * 3 + 1];
		int i2 = mesh.indices[i * 3 + 2];
        
        //TODO pre backface cull
        //if (IsBackFace(vertices[i0].position, vertices[i1].position, vertices[i2].position)) continue;
        
        //TODO Guard-band clipping
        auto triangles = Clipper::ClipTriangle(vertices[i0], vertices[i1], vertices[i2]);
        for (auto triangle : triangles)
        {
			Projection p0 = triangle.v0.GetViewProjection(width, height);
			Projection p1 = triangle.v1.GetViewProjection(width, height);
			Projection p2 = triangle.v2.GetViewProjection(width, height);
            
            if (Orient2D(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y) < 0.f) continue;
            
            DrawTriangle(p0, p1, p2, triangle);
        }
	}
}

}