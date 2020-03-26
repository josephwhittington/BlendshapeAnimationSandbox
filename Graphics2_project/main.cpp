// Graphics2_project.cpp : Defines the entry point for the application.

#include <vector>
#include <string>
#include <fstream>

#include "framework.h"
#include "Graphics2_project.h"

#include <d3d11.h>
#include <DirectXMath.h>

#include "assets/StoneHenge.h"
#include "Camera.h"
#include "Enums_globals.h"

#include "basic_vs.csh"
#include "basic_ps.csh"

#include "base_vs.csh"
#include "base_ps.csh"

#include "henge_vs.csh"

#include "XTime.h"

using namespace DirectX;

// Globals
// For initialization and utility
ID3D11Device* device;
IDXGISwapChain* swapchain;
ID3D11DeviceContext* deviceContext;

// States
ID3D11RasterizerState* rasterizerStateDefault;
ID3D11RasterizerState* rasterizerStateWireframe;

// For drawing
ID3D11RenderTargetView* renderTargetView;
D3D11_VIEWPORT viewport;
float aspectRatio = 1;

// Shader variables
ID3D11Buffer* constantBuffer;
// Shader variables

// Z buffer
ID3D11Texture2D* zBuffer;
ID3D11DepthStencilView* depthStencil;
// Z buffer

// Camera shit
FPSCamera camera;

bool g_wireframeEnabled = false;

// Object shit
struct Model
{
	std::vector<SimpleVertex> vertices;
	std::vector<SimpleVertex> vs_verts;
	std::vector<int> indices;
	std::vector<FLOAT3> blend_pose;

	XMFLOAT3 position;

	ID3D11Buffer* vertexBuffer;
	ID3D11Buffer* indexBuffer;

	ID3D11VertexShader* vertexShader;
	ID3D11PixelShader* pixelShader;
	ID3D11InputLayout* vertexBufferLayout;

	// Aniamtion
	float time = 0;
	bool flip = false;
};

struct BUFFERS
{
	ID3D11Buffer* c_bufer;
};

// Math globals
struct WorldViewProjection
{
	XMFLOAT4X4 WorldMatrix;
	XMFLOAT4X4 ViewMatrix;
	XMFLOAT4X4 ProjectionMatrix;
}WORLD;	
// Models
Model model;
BUFFERS buffers;
XTime _time;

// Globals

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


// FUNCTION DECLARATION

void HandleInput();
void LoadWobjectMesh(const char* meshname, std::vector<SimpleVertex>& vertices, std::vector<int>& indices);
void InitializeModel(Model& model, const char* modelname, XMFLOAT3 position, const BYTE* _vs, const BYTE* _ps, int vs_size, int ps_size);
void CleanupModel(Model& model);

void Lerpposition();
float lerp(float start, float end, float ratio);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_GRAPHICS2PROJECT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_GRAPHICS2PROJECT));

    MSG msg;
	_time.Restart();
    // Main message loop:
	while (true)
	{
		_time.Signal();
		if(model.flip)
			model.time += _time.SmoothDelta();
		else model.time -= _time.SmoothDelta();
		Lerpposition();

		PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT)
			break;
		if (GetAsyncKeyState(VK_ESCAPE)) break;

		// Input shit
		HandleInput();

		// DONT FUCK WITH THIS
		// Rendering shit right here
		// Output merger
		ID3D11RenderTargetView* tempRTV[] = { renderTargetView };
		deviceContext->OMSetRenderTargets(1, tempRTV, depthStencil);

		float color[4] = { 0, 0, 0, 1 };
		deviceContext->ClearRenderTargetView(renderTargetView, color);
		deviceContext->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH, 1, 0);

		// Rasterizer
		deviceContext->RSSetViewports(1, &viewport);
		// DONT FUCK WITH THIS

		// TEAPOT
		// World matrix projection
		XMMATRIX temp = XMMatrixIdentity();
		temp = XMMatrixMultiply(temp, XMMatrixScaling(1, 1, 1));
		temp = XMMatrixMultiply(temp, XMMatrixTranslation(model.position.x, model.position.y, model.position.z));
		XMStoreFloat4x4(&WORLD.WorldMatrix, temp);
		// View
		camera.GetViewMatrix(temp);
		XMStoreFloat4x4(&WORLD.ViewMatrix, temp);
		// Projection
		temp = XMMatrixPerspectiveFovLH(camera.GetFOV(), aspectRatio, 0.1f, 1000);
		XMStoreFloat4x4(&WORLD.ProjectionMatrix, temp);

		// Send the matrix to constant buffer
		D3D11_MAPPED_SUBRESOURCE gpuBuffer;
		HRESULT result = deviceContext->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &gpuBuffer);
		memcpy(gpuBuffer.pData, &WORLD, sizeof(WORLD));
		deviceContext->Unmap(constantBuffer, 0);
		// Connect constant buffer to the pipeline
		ID3D11Buffer* teapotCBuffers[] = { constantBuffer };
		deviceContext->VSSetConstantBuffers(0, 1, teapotCBuffers);

		deviceContext->UpdateSubresource(model.vertexBuffer, 0, NULL, model.vs_verts.data(), 0, 0);

		// sET THE PIPELINE
		UINT teapotstrides[] = { sizeof(SimpleVertex) };
		UINT teapotoffsets[] = { 0 };
		ID3D11Buffer* teapotVertexBuffers[] = { model.vertexBuffer };
		deviceContext->IASetVertexBuffers(0, 1, teapotVertexBuffers, teapotstrides, teapotoffsets);
		deviceContext->IASetIndexBuffer(model.indexBuffer, DXGI_FORMAT_R32_UINT, 0);
		deviceContext->VSSetShader(model.vertexShader, 0, 0);
		deviceContext->PSSetShader(model.pixelShader, 0, 0);
		deviceContext->IASetInputLayout(model.vertexBufferLayout);

		deviceContext->DrawIndexed(model.indices.size(), 0, 0);
		// TEAPOT

		swapchain->Present(0, 0);
    }

	// Release a million fucking things
	D3DSAFE_RELEASE(swapchain);
	D3DSAFE_RELEASE(deviceContext);
	D3DSAFE_RELEASE(device);
	D3DSAFE_RELEASE(renderTargetView);
	D3DSAFE_RELEASE(constantBuffer);

	D3DSAFE_RELEASE(rasterizerStateDefault);
	D3DSAFE_RELEASE(rasterizerStateWireframe);

	D3DSAFE_RELEASE(zBuffer);
	D3DSAFE_RELEASE(depthStencil);

	CleanupModel(model);

    return (int) msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_GRAPHICS2PROJECT));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_GRAPHICS2PROJECT);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(
	   szWindowClass, 
	   szTitle,
	   WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
	   CW_USEDEFAULT, 
	   0, 
	   CW_USEDEFAULT, 
	   0, 
	   nullptr, 
	   nullptr, 
	   hInstance, 
	   nullptr);


   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   RECT rect;
   GetClientRect(hWnd, &rect);

   // Attach d3d to the window
   D3D_FEATURE_LEVEL DX11 = D3D_FEATURE_LEVEL_11_0;
   DXGI_SWAP_CHAIN_DESC swap;
   ZeroMemory(&swap, sizeof(DXGI_SWAP_CHAIN_DESC));
   swap.BufferCount = 1;
   swap.OutputWindow = hWnd;
   swap.Windowed = true;
   swap.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
   swap.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
   swap.BufferDesc.Width = rect.right - rect.left;
   swap.BufferDesc.Height = rect.bottom - rect.top;
   swap.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
   swap.SampleDesc.Count = 1;

   aspectRatio = swap.BufferDesc.Width / (float)swap.BufferDesc.Height;

   HRESULT result;

   result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG, &DX11, 1, D3D11_SDK_VERSION, &swap, &swapchain, &device, 0, &deviceContext);


   ID3D11Resource* backbuffer;
   result = swapchain->GetBuffer(0, __uuidof(backbuffer), (void**)&backbuffer);
   result = device->CreateRenderTargetView(backbuffer, NULL, &renderTargetView);

   // Release the resource to decrement the counter by one
   // This is necessary to keep the thing from leaking memory
   backbuffer->Release();

   // Setup viewport
   viewport.Width = swap.BufferDesc.Width;
   viewport.Height = swap.BufferDesc.Height;
   viewport.TopLeftY = viewport.TopLeftX = 0;
   viewport.MinDepth = 0;
   viewport.MaxDepth = 1;

   // Rasterizer state
   D3D11_RASTERIZER_DESC rdesc;
   ZeroMemory(&rdesc, sizeof(D3D11_RASTERIZER_DESC));
   rdesc.FrontCounterClockwise = false;
   rdesc.DepthBiasClamp = 1;
   rdesc.DepthBias = rdesc.SlopeScaledDepthBias = 0;
   rdesc.DepthClipEnable = true;
   rdesc.FillMode = D3D11_FILL_SOLID;
   rdesc.CullMode = D3D11_CULL_BACK;
   rdesc.AntialiasedLineEnable = false;
   rdesc.MultisampleEnable = false;

   device->CreateRasterizerState(&rdesc, &rasterizerStateDefault);

   // Wire frame Rasterizer State
   ZeroMemory(&rdesc, sizeof(D3D11_RASTERIZER_DESC));
   rdesc.FillMode = D3D11_FILL_WIREFRAME;
   rdesc.CullMode = D3D11_CULL_NONE;
   rdesc.DepthClipEnable = true;

   device->CreateRasterizerState(&rdesc, &rasterizerStateWireframe);

   deviceContext->RSSetState(rasterizerStateDefault);

   // Initialize camera
   camera.SetPosition(XMFLOAT3(0, 0, -5));
   camera.SetFOV(30);

   // TEAPOT
   //InitializeModel(model, "assets/models/bunny.wobj", XMFLOAT3(0, -1, 0), base_vs, base_ps, sizeof(base_vs), sizeof(base_ps));
   InitializeModel(model, "assets/models/blend_cube.wobj", XMFLOAT3(0, -1, 0), base_vs, base_ps, sizeof(base_vs), sizeof(base_ps));
   // TEAPOT

   deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   // Create constant buffer
   D3D11_BUFFER_DESC bDesc;
   D3D11_SUBRESOURCE_DATA subdata;
   ZeroMemory(&bDesc, sizeof(D3D11_BUFFER_DESC));
   ZeroMemory(&subdata, sizeof(D3D11_SUBRESOURCE_DATA));

   bDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
   bDesc.ByteWidth = sizeof(WorldViewProjection);
   bDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
   bDesc.MiscFlags = 0;
   bDesc.StructureByteStride = 0;
   bDesc.Usage = D3D11_USAGE_DYNAMIC;

   result = device->CreateBuffer(&bDesc, nullptr, &constantBuffer);
   ASSERT_HRESULT_SUCCESS(result);

   // Z buffer 
   D3D11_TEXTURE2D_DESC zDesc;
   ZeroMemory(&zDesc, sizeof(zDesc));
   zDesc.ArraySize = 1;
   zDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
   zDesc.Width = swap.BufferDesc.Width;
   zDesc.Height = swap.BufferDesc.Height;
   zDesc.Usage = D3D11_USAGE_DEFAULT;
   zDesc.Format = DXGI_FORMAT_D32_FLOAT;
   zDesc.MipLevels = 1;
   zDesc.SampleDesc.Count = 1;

   result = device->CreateTexture2D(&zDesc, nullptr, &zBuffer);

   D3D11_DEPTH_STENCIL_DESC zViewDesc;
   ZeroMemory(&zViewDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));

   result = device->CreateDepthStencilView(zBuffer, nullptr, &depthStencil);

   assert(!FAILED(result));
   return TRUE;
}

void CleanupModel(Model& model)
{
	// teapot cleanup
	D3DSAFE_RELEASE(model.indexBuffer);
	D3DSAFE_RELEASE(model.pixelShader);
	D3DSAFE_RELEASE(model.vertexBuffer);
	D3DSAFE_RELEASE(model.vertexBufferLayout);
	D3DSAFE_RELEASE(model.vertexShader);
	// teapot cleanup
}

void InitializeModel(Model& model, const char* modelname, XMFLOAT3 position, const BYTE* _vs, const BYTE* _ps, int vs_size, int ps_size)
{
	// TEAPOT
   // Set position
	model.position = position;
	// Load mesh data
	D3D11_BUFFER_DESC bufferDesc;
	D3D11_SUBRESOURCE_DATA subdata;
	ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));
	ZeroMemory(&subdata, sizeof(D3D11_SUBRESOURCE_DATA));
	// Actually load data
	LoadWobjectMesh(modelname, model.vertices, model.indices);
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.ByteWidth = sizeof(SimpleVertex) * model.vertices.size();
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = 0;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	subdata.pSysMem = model.vertices.data();

	HRESULT result = device->CreateBuffer(&bufferDesc, &subdata, &model.vertexBuffer);
	ASSERT_HRESULT_SUCCESS(result);

	// Index Buffer
	bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufferDesc.ByteWidth = sizeof(int) * model.indices.size();

	subdata.pSysMem = model.indices.data();
	result = device->CreateBuffer(&bufferDesc, &subdata, &model.indexBuffer);
	ASSERT_HRESULT_SUCCESS(result);

	// Load shaders
	result = device->CreateVertexShader(_vs, vs_size, nullptr, &model.vertexShader);
	ASSERT_HRESULT_SUCCESS(result);
	result = device->CreatePixelShader(_ps, ps_size, nullptr, &model.pixelShader);
	ASSERT_HRESULT_SUCCESS(result);

	// Make input layout for vertex buffer
	D3D11_INPUT_ELEMENT_DESC tempInputElementDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	result = device->CreateInputLayout(tempInputElementDesc, 3, _vs, vs_size, &model.vertexBufferLayout);
	ASSERT_HRESULT_SUCCESS(result);
	// TEAPOT
}

void LoadWobjectMesh(const char* meshname, std::vector<SimpleVertex>& vertices, std::vector<int>& indices)
{
	MeshHeader header;

	std::string smeshname = meshname;

	assert(smeshname.find(".fbx") == std::string::npos);

	std::ifstream file;
	file.open(meshname, std::ios::binary | std::ios::in);
	assert(file.is_open());

	// Read in the header
	file.read((char*)&header, sizeof(MeshHeader));
	std::string s = header.texturename;

	// Create a buffer to hold the data
	char* buffer = new char[header.vertexcount * (size_t)sizeof(SimpleVertex)];

	// Read in the indices
	file.read((char*)buffer, header.indexcount * (size_t)sizeof(int));
	indices.resize(header.indexcount);
	memcpy(indices.data(), buffer, sizeof(int) * header.indexcount);

	// Read in the vertices
	file.read((char*)buffer, header.vertexcount * (size_t)sizeof(SimpleVertex));
	vertices.resize(header.vertexcount);
	memcpy(vertices.data(), buffer, sizeof(SimpleVertex) * header.vertexcount);

	// Read in the blend position
	file.read(buffer, sizeof(FLOAT3) * header.vertexcount);
	model.blend_pose.resize(header.vertexcount);
	memcpy(model.blend_pose.data(), buffer, sizeof(FLOAT3) * header.vertexcount);

	// Eh
	for (int i = 0; i < model.blend_pose.size(); i++)
	{
		//model.vertices[i].Pos = model.blend_pose[i];
	}
	// Eh

	// Load verts
	model.vs_verts.assign(model.vertices.begin(), model.vertices.end());

	// Free memory
	delete[] buffer;

	file.close();
}

float lerp(float start, float end, float ratio)
{
	return start + ratio * (end - start);
}

void Lerpposition()
{
	float current_time = model.time;
	float lerp_ratio = current_time / .4;

	if (current_time > .4 || current_time < 0) model.flip = !model.flip;

	if (lerp_ratio > 1) lerp_ratio = 1;
	if (lerp_ratio < 0) lerp_ratio = 0;
	// 1 second animation
	for (int i = 0; i < model.blend_pose.size(); i++)
	{
		model.vs_verts[i].Pos.x = lerp(model.vertices[i].Pos.x, model.blend_pose[i].x, lerp_ratio);
		model.vs_verts[i].Pos.y = lerp(model.vertices[i].Pos.y, model.blend_pose[i].y, lerp_ratio);
		model.vs_verts[i].Pos.z = lerp(model.vertices[i].Pos.z, model.blend_pose[i].z, lerp_ratio);
	}
}

void HandleInput()
{
	static float move_thresh = .01;
	static float look_thresh = .03;
	XMFLOAT3 direction;

	if (GetAsyncKeyState((int)BUTTONS::NUM_ONE))
	{
		deviceContext->RSSetState(rasterizerStateWireframe);
	}
	else if (GetAsyncKeyState((int)BUTTONS::NUM_TWO))
	{
		deviceContext->RSSetState(rasterizerStateDefault);
	}
	// Movement
	if (GetAsyncKeyState((int)BUTTONS::LETTER_W))
	{
		direction = camera.GetLook();
		direction.x *= move_thresh;
		direction.y *= move_thresh;
		direction.z *= move_thresh;
		camera.Move(direction);
	}
	else if (GetAsyncKeyState((int)BUTTONS::LETTER_S))
	{
		direction = camera.GetLook();
		direction.x *= -move_thresh;
		direction.y *= -move_thresh;
		direction.z *= -move_thresh;
		camera.Move(direction);
	} 
	else if (GetAsyncKeyState((int)BUTTONS::LETTER_A))
	{
		direction = camera.GetRight();
		direction.x *= move_thresh;
		direction.y *= move_thresh;
		direction.z *= move_thresh;
		camera.Move(direction);
	}
	else if (GetAsyncKeyState((int)BUTTONS::LETTER_D))
	{
		direction = camera.GetRight();
		direction.x *= -move_thresh;
		direction.y *= -move_thresh;
		direction.z *= -move_thresh;
		camera.Move(direction);
	}
	else if (GetAsyncKeyState((int)BUTTONS::LETTER_Z))
	{
		camera.Move(XMFLOAT3(0, move_thresh, 0));
	}
	else if (GetAsyncKeyState((int)BUTTONS::LETTER_X))
	{
		camera.Move(XMFLOAT3(0, -move_thresh, 0));
	}

	// Looking
	if (GetAsyncKeyState(VK_UP))
	{
		camera.Rotate(0, look_thresh);
	}
	else if (GetAsyncKeyState(VK_DOWN))
	{
		camera.Rotate(0, -look_thresh);
	}
	else if (GetAsyncKeyState(VK_LEFT))
	{
		camera.Rotate(look_thresh, 0);
	}
	else if (GetAsyncKeyState(VK_RIGHT))
	{
		camera.Rotate(-look_thresh, 0);
	}
}


//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    //case WM_PAINT:
    //    {
    //        PAINTSTRUCT ps;
    //        HDC hdc = BeginPaint(hWnd, &ps);
    //        // TODO: Add any drawing code that uses hdc here...
    //        EndPaint(hWnd, &ps);
    //    }
    //    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}