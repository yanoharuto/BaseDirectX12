
#include <Windows.h>
#ifdef _DEBUG
#include <iostream>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <functional>
#include <DirectXMath.h>
#include "Window.h"
#include "FilePrint.h"
#include <d3dcompiler.h>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")// ディスプレイ出力機能の制御に使うAPIをインクルード
#pragma comment(lib, "d3dcompiler.lib") // シェーダーコンパイルに必要なライブラリ
#endif
using namespace std;
using namespace DirectX;