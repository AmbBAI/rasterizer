#include "rasterizer.h"
using namespace rasterizer;

Application* app;
Canvas* canvas;

void MainLoop();

int main(int argc, char *argv[])
{
	app = Application::GetInstance();
	app->CreateApplication(argc, argv, "rasterizer", 512, 512);
	canvas = app->GetCanvas();
	app->SetRunLoop(MainLoop);
	app->RunLoop();
	return 0;
}

void MainLoop()
{
	Camera camera;
	camera.SetLookAtLH(Vector3(0, -50, 100), Vector3(0, 0, 0), Vector3::up);
	camera.SetProjectionMatrix(Matrix4x4::Perspective(90, 1, 0.3f, 100));
	std::vector<Mesh> meshes;
	Mesh::LoadMesh(meshes, "resources/teapot/teapot.obj");

	float r = 0;
	while (true)
	{
		r += 0.1f;
		canvas->BeginDraw();

		Matrix4x4 trans(Vector3(0, 0, 0), Quaternion(Vector3(r, r, 0)), Vector3(1, 1, 1));

		for (int i = 0; i < (int)meshes.size(); ++i)
		{
			Rasterizer::DrawMeshPoint(canvas, camera, meshes[i], trans);
		}

		//Rasterizer::Line(canvas, Color(1, 1, 1, 1), 300, 210, 10, 100);
		//Rasterizer::Line(canvas, Color(0.6f, 0.5f, 1, 1), 400, 220, 10, 100);
		//Rasterizer::Line(canvas, Color(0.2f, 1, 1, 1), 400, 220, 100, 100);
		//Rasterizer::Line(canvas, Color(1, 1, 1, 1), 220, 399, -100, 400);
		//Rasterizer::Line(canvas, Color(1, 1, 0.5f, 1), 270, 270, 150, 400);

		//Rasterizer::SmoothLine(canvas, Color(1, 1, 1, 1), 100, 10, 10, 100);
		//Rasterizer::SmoothLine(canvas, Color(0.6f, 0.5f, 1, 1), 200, 20, 10, 100);
		//Rasterizer::SmoothLine(canvas, Color(0.2f, 1, 1, 1), 200, 20, 100, 100);
		//Rasterizer::SmoothLine(canvas, Color(1, 1, 1, 1), 20, 199, -100, 400);
		//Rasterizer::SmoothLine(canvas, Color(1, 1, 0.5f, 1), 70, 70, 150, 400);



		canvas->EndDraw();
	}

}