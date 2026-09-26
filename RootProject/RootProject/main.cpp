
#include "main.h"

using namespace DirectX;

// ウィンドウに対する操作（閉じる、リサイズなど）を処理する関数
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DESTROY:
        PostQuitMessage(0); // ウィンドウが閉じられたらプログラムを終了
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void EnableDebugLayer() {
    ID3D12Debug* debugLayer = nullptr;
    auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
    debugLayer->EnableDebugLayer(); // デバッグレイヤー を 有効 化 する 
    debugLayer->Release(); // 有効 化 し たら インター フェイス を 解放 する 
}

// メイン関数（Windowsアプリのエントリポイント）
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1. ウィンドウの情報を設定
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DX12WindowClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClassEx(&wc);

    // 2. ウィンドウを作成
    HWND hwnd = CreateWindowEx(
        0, L"DX12WindowClass", L"DirectX12 Game Window",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        nullptr, nullptr, hInstance, nullptr
    );

    if (hwnd == nullptr) return 0;

    ShowWindow(hwnd, nCmdShow);

#ifdef _DEBUG
	EnableDebugLayer(); // デバッグレイヤーを有効化（デバッグビルド時のみ）
#endif

	// DirectX 12の初期化（デバイス、スワップチェーンなど）
    ID3D12Device* _dev = nullptr;
    IDXGIFactory6* _dxgiFactory = nullptr;

	// DirectX 12デバイスの作成
	D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL featureLevel;
	// フィーチャーレベルが合わないとデバイス作成が失敗するのでFor文で順番に試す
    for (int i = 0; i < _countof(levels); ++i) {
        if (SUCCEEDED(D3D12CreateDevice(nullptr, levels[i], IID_PPV_ARGS(&_dev)))) {
            featureLevel = levels[i];
            break;
        }
    }

	IDXGIAdapter1* baseAdapter = nullptr; // bstAdapterと比較して、より良いアダプターを選ぶための基準
    IDXGIAdapter4* bstAdapter = nullptr; // DX12では最新のIDXGIAdapter4を使うのが望ましい

    // CreateDXGIFactory2を使い、デバッグレイヤーが有効な場合は
    // DXGI_CREATE_FACTORY_DEBUG フラグを渡せるようにするとデバッグが快適になります
#ifdef _DEBUG
    auto result = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
    IID_PPV_ARGS(&_dxgiFactory);
#else
	CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
#endif

    // ループ内で直接評価し、メモリリークを防ぐ（Releaseの管理）
    for (UINT i = 0; _dxgiFactory->EnumAdapters1(i, &baseAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 adesc = {};
        baseAdapter->GetDesc1(&adesc);

        // ソフトウェアレンダラー（Microsoft Basic Render Driverなど）はゲーム用ではないのでスキップ
        if (adesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            baseAdapter->Release();
            continue;
        }

        // 基本は「ビデオメモリ（DedicatedVideoMemory）が一番大きいもの」を最優先で選ぶ
        // これにより、CPU内蔵グラフィックスではなく、確実にNVIDIAなどの外付けGPUが選ばれる
        if (bstAdapter == nullptr) {
            baseAdapter->QueryInterface(IID_PPV_ARGS(&bstAdapter));
        }
        else {
            DXGI_ADAPTER_DESC1 currentDesc = {};
            bstAdapter->GetDesc1(&currentDesc);
            if (adesc.DedicatedVideoMemory > currentDesc.DedicatedVideoMemory) {
                bstAdapter->Release();
                baseAdapter->QueryInterface(IID_PPV_ARGS(&bstAdapter));
            }
        }
		baseAdapter->Release(); //ループ内で作成したアダプターは不要になったら解放
    }

    std::vector<std::function<void(void)>> commandList; //遅延実行できる命令リスト
    ID3D12CommandAllocator* _cmdAllocator = nullptr; 
    ID3D12GraphicsCommandList* _cmdList = nullptr;
    result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator));
    result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList));
    _cmdList->Close();

    // ID3D12
	ID3D12CommandQueue* _cmdQueue = nullptr;
    D3D12_COMMAND_QUEUE_DESC _cmdQueueDesc = {};
    // タイムアウトなし
	_cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	_cmdQueueDesc.NodeMask = 0; // 0はシングルGPU環境での使用を意味する
	_cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	_cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // CreateCommandAllocator関数とかで指定したのと同じタイプを指定する必要がある
    result = _dev->CreateCommandQueue(&_cmdQueueDesc, IID_PPV_ARGS(&_cmdQueue));

    // スワップチェーン用意
    // ダブルバッファリング（描画切り替え）をできるようになるために必要
    IDXGISwapChain4* _swapChain = nullptr;
    DXGI_SWAP_CHAIN_DESC1 _swapChainDesc = {};
    _swapChainDesc.BufferCount = 2; //　描画を2枚用意して切り替え出来るように
    _swapChainDesc.Width = WINDOW_W;
    _swapChainDesc.Height = WINDOW_H;
    _swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    _swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
    _swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    _swapChainDesc.SampleDesc.Count = 1;
    _swapChainDesc.Stereo = false;
    // バックバッファー は 伸び 縮み 可能
    _swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    // フリップ 後 は 速やか に 破棄 
    _swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; 
    // 特に 指定 なし 
    _swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    // ウィンドウ ⇔ フル スクリーン 切り替え 可能 
    _swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; 

	HRESULT hr = _dxgiFactory->CreateSwapChainForHwnd(
		_cmdQueue, hwnd, &_swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&_swapChain)
	);

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {}; // バッファーに書き込む用のメモリ確保
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビュー用のヒープ
	heapDesc.NodeMask = 0; // シングルGPU環境での使用を意味する
    heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // CPUからアクセスするのでNONE
	
    ID3D12DescriptorHeap* _rtvHeaps = nullptr;
	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&_rtvHeaps));

    // スワップチェーンのバックバッファーにアクセスする
    DXGI_SWAP_CHAIN_DESC swcDesc = {};
    _swapChain->GetDesc(&swcDesc);
    std::vector<ID3D12Resource*> _backBuffers(swcDesc.BufferCount);
    for (int idx = 0; idx < swcDesc.BufferCount; ++idx) {
		result = _swapChain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx]));
	
        D3D12_CPU_DESCRIPTOR_HANDLE handle = _rtvHeaps->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += idx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); // 1つ目は先頭のアドレスでよいが、2つ目以降のバッファーを取得するのにディスクリプタのサイズの計算が必要
		_dev->CreateRenderTargetView(_backBuffers[idx], nullptr, handle);
    }

    // フェンス作るよ
    ID3D12Fence* _fence = nullptr;
    UINT64 _fenceValue = 0;
    result = _dev->CreateFence(_fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

	XMFLOAT3 vertices[] = {
		{ -1.2f, -0.9f, 0.0f }, // 上の頂点
		{ 1.0f, 0.3f, 0.0f }, // 右下の頂点
		{ 1.0f, 1.5f, 0.0f } // 左下の頂点
	};

    // 頂点バッファー作成
    D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD; // CPUから書き込むのでUPLOAD
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resdesc = {};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = 1024; // 頂点データのサイズ
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.SampleDesc.Quality = 0;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* vertBuff = nullptr;

	result = _dev->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff)
	);

	XMFLOAT3* vertMap = nullptr;
	result = vertBuff->Map(0, nullptr, reinterpret_cast<void**>(&vertMap));
	std::copy(std::begin(vertices), std::end(vertices), vertMap); // 頂点データをコピー
    vertBuff->Unmap(0, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbView = {};

	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress(); // 頂点バッファーのGPU仮想アドレスを取得
    vbView.SizeInBytes = sizeof(vertices);
	vbView.StrideInBytes = sizeof(vertices[0]);

	// シェーダーのコンパイル
	ID3DBlob* vertexShaderBlob = nullptr;
	ID3DBlob* pixelShaderBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	result = D3DCompileFromFile(L"BasicVertexShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "BasicVS", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &vertexShaderBlob, &errorBlob);

    result = D3DCompileFromFile(L"BasicPixelShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "BasicPS", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &pixelShaderBlob, &errorBlob);

	if (FAILED(result)) {
        if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
            ::OutputDebugStringA("シェーダーファイルが見つかりませんでした。\n");
            return 0; // 終了プログラム別途必要
        }
        else {
            std::string errstr = "シェーダーのコンパイルに失敗しました: ";
            errstr.resize(errorBlob->GetBufferSize());
            std::copy_n(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize(), errstr.begin());
            errstr += "\n";
            ::OutputDebugStringA(errstr.c_str());
        }
	}

	// パイプラインステートの設定

    D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = nullptr; // ルートシグネチャは後で設定する
    gpipeline.VS.pShaderBytecode = vertexShaderBlob->GetBufferPointer();
	gpipeline.VS.BytecodeLength = vertexShaderBlob->GetBufferSize();
	gpipeline.PS.pShaderBytecode = pixelShaderBlob->GetBufferPointer();
	gpipeline.PS.BytecodeLength = pixelShaderBlob->GetBufferSize();

    gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gpipeline.RasterizerState.MultisampleEnable = FALSE; // アンチエイリアスを使用しない
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // カリングしない
	gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; // 塗りつぶし
	gpipeline.RasterizerState.DepthClipEnable = TRUE; // 深度クリッピングを有効にする


	// レンダーターゲットのブレンドステートを設定
    D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
    renderTargetBlendDesc.BlendEnable = false;
    renderTargetBlendDesc.LogicOpEnable = false;
    renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    gpipeline.BlendState.AlphaToCoverageEnable = false;
    gpipeline.BlendState.IndependentBlendEnable = false;
    gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;

    // 入力レイアウトの設定
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {
            "POSITION", // セマンティクス
            0,// POSITIONセマンティクスのインデックス
            DXGI_FORMAT_R32G32B32_FLOAT, // フォーマット
            0, //　入力スロット
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        }
    };
    gpipeline.InputLayout.pInputElementDescs = inputLayout;
	gpipeline.InputLayout.NumElements = _countof(inputLayout);

	// プリミティブトポロジーの設定 ポリゴンの表現方法を指定する
	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; 
	// レンダーターゲットのフォーマットを指定する
	gpipeline.NumRenderTargets = 1;
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    // アンチエイリアシングのサンプル
	gpipeline.SampleDesc.Count = 1; // マルチサンプリングしない
	gpipeline.SampleDesc.Quality = 0;
	ID3D12PipelineState* _pipelineState = nullptr;

	
	// ルートシグネチャの作成
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ID3DBlob* _rootSigBlob = nullptr;
	result = D3D12SerializeRootSignature(
        &rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1, // ルートシグネチャのバージョンを指定
        &_rootSigBlob,
        &errorBlob
    );
    ID3D12RootSignature* _rootSignature = nullptr;
    result = _dev->CreateRootSignature(
		0, // ノードマスク（シングルGPU環境では0）
		_rootSigBlob->GetBufferPointer(),
		_rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(&_rootSignature)
	);
	_rootSigBlob->Release(); // ルートシグネチャのバイナリは不要になったら解放

	gpipeline.pRootSignature = _rootSignature; // ルートシグネチャをパイプラインステートに設定
    result = _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&_pipelineState));

	// ビューポートとシザー矩形の設定
	D3D12_VIEWPORT viewport = {};
	viewport.Width = static_cast<float>(WINDOW_W);
	viewport.Height = static_cast<float>(WINDOW_H);
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;

	// シザー矩形(描画範囲)の設定
	D3D12_RECT scissorrect = {};
    scissorrect.top = 0;
    scissorrect.left = 0;
	scissorrect.right = scissorrect.left +  WINDOW_W;
	scissorrect.bottom = scissorrect.top + WINDOW_H;


    // 3. メインループ（ゲームループの基礎）
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            _cmdAllocator->Reset();
            _cmdList->Reset(_cmdAllocator, nullptr);


            // レンダーターゲットを指定する
            auto bbIdx = _swapChain->GetCurrentBackBufferIndex();
			auto rtvH = _rtvHeaps->GetCPUDescriptorHandleForHeapStart(); // レンダーターゲットビューのヒープの先頭アドレスを取得
            rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            _cmdList->OMSetRenderTargets(1, &rtvH, true, nullptr);

            // バリアを作る
            D3D12_RESOURCE_BARRIER barrierDesc = {};
            barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrierDesc.Transition.pResource = _backBuffers[bbIdx];
            barrierDesc.Transition.Subresource = 0;

			barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			_cmdList->ResourceBarrier(1, &barrierDesc); // 書き込みモードにせよってコマンドを送る

			// 三角形を描画するためのコマンドを送る
            _cmdList->SetPipelineState(_pipelineState);
			_cmdList->SetGraphicsRootSignature(_rootSignature);
			_cmdList->RSSetViewports(1, &viewport);
            _cmdList->RSSetScissorRects(1, &scissorrect);
			_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 三角形リストとして描画する
			_cmdList->IASetVertexBuffers(0, 1, &vbView);
			_cmdList->DrawInstanced(3, 1, 0, 0); // 頂点数、インスタンス数、開始頂点、開始インスタンス


            barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            _cmdList->ResourceBarrier(1, &barrierDesc); // 表示モードにせよってコマンドを送る

			_cmdList->Close();
			// コマンドリストを実行する
			ID3D12CommandList* cmdLists[] = { _cmdList };
			_cmdQueue->ExecuteCommandLists(1, cmdLists);
			_cmdQueue->Signal(_fence, ++_fenceValue);

			if (_fence->GetCompletedValue() != _fenceValue) {
				auto event = CreateEvent(nullptr, false, false, nullptr);
				_fence->SetEventOnCompletion(_fenceValue, event);
				WaitForSingleObject(event, INFINITE);
				CloseHandle(event);
			}

			_swapChain->Present(1, 0); // VSync有効


        }
    }

    return 0;
}
