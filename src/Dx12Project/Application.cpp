#include "framework.h" // Windowsの標準的なinclude
#include "Application.h"
#include <random>

namespace tpot
{
	Application::Application(const ApplicationArgs& 引数) : 
		幅_(引数.幅), 高さ_(引数.高さ), Warpデバイス使用_(引数.useWarpDevice),
		アスペクト比_(static_cast<float>(引数.幅) / static_cast<float>(引数.高さ)),
        ビューポート_(0.0f, 0.0f, static_cast<float>(引数.幅), static_cast<float>(引数.高さ)),
        はさみ矩形_(0, 0, static_cast<LONG>(引数.幅), static_cast<LONG>(引数.高さ))
    {
    }
	
	Application::~Application() 
	{
	}

    int Application::Initialize(HWND ウィンドウハンドル)
    {
        パイプラインの読み込み(ウィンドウハンドル, 幅_, 高さ_);
        アセットの読み込み();

        return 0;
    }

    void Application::Finalize()
    {
        // GPUが、デストラクタによってクリーンアップされようとしているリソースを
        // 参照しなくなっていることを確認
        GPUを待つ();

        CloseHandle(フェンスイベント_);
    }

    
    int Application::Update()
    {
        // ■追加：オブジェクトの動きの更新
        angle += 0.1f;
		if (DirectX::XM_2PI <= angle) angle -= DirectX::XM_2PI;//360 度を超えないようにする

        // シーンのレンダリングに必要なコマンドをすべてコマンドリストに記録
        コマンドリストの記録();

        // コマンドリストを実行
        ID3D12CommandList* pコマンドリスト列[] = { コマンドリスト_.get() };
        コマンドキュー_->ExecuteCommandLists(
            _countof(pコマンドリスト列), pコマンドリスト列);

        // フレームを(画面に)提示する
        winrt::check_hresult(スワップチェーン_->Present(1, 0));

        次のフレームに行く();

        return 0;
    }

    void Application::パイプラインの読み込み(HWND ウィンドウハンドル, 
        unsigned int 幅, unsigned int 高さ)
	{
        UINT dxgiファクトリーフラグ = 0;

#if defined(_DEBUG)
        // デバッグレイヤーを有効にする（グラフィックツールの「オプション機能」が必要）
        // 注意：デバイス作成後にデバッグ・レイヤーを有効にすると、アクティブ・デバイス
        // は無効になる
        {
            winrt::com_ptr<ID3D12Debug> デバッグコントローラー;
            if (SUCCEEDED(D3D12GetDebugInterface(
                __uuidof(デバッグコントローラー), デバッグコントローラー.put_void())))
            {
                デバッグコントローラー->EnableDebugLayer();

                // 追加のデバッグレイヤを有効化
                dxgiファクトリーフラグ |= DXGI_CREATE_FACTORY_DEBUG;
            }
        }
#endif
        // DXGI オブジェクトを生成する
        winrt::com_ptr<IDXGIFactory4> ファクトリー;
        winrt::check_hresult(CreateDXGIFactory2(dxgiファクトリーフラグ, 
            __uuidof(ファクトリー), ファクトリー.put_void()));

		// デバイスを作成
        if (Warpデバイス使用_)
        {
            winrt::com_ptr<IDXGIAdapter> WARPアダプター;
            winrt::check_hresult(ファクトリー->EnumWarpAdapter(
                __uuidof(WARPアダプター), WARPアダプター.put_void()));

            winrt::check_hresult(D3D12CreateDevice(
                WARPアダプター.get(),
                D3D_FEATURE_LEVEL_12_0,// DX12の機能レベルに決め打ち
                __uuidof(デバイス_), デバイス_.put_void()
            ));
        }
        else
        {
            winrt::com_ptr<IDXGIAdapter1> ハードウェアアダプター;
            ハードウェアアダプターの取得(ファクトリー.get(), 
                ハードウェアアダプター.put());

            winrt::check_hresult(D3D12CreateDevice(
                ハードウェアアダプター.get(),
                D3D_FEATURE_LEVEL_12_0,// DX12の機能レベルに決め打ち
                __uuidof(デバイス_), デバイス_.put_void()
            ));
        }

        // コマンドキューについて記述し、作成する。
        D3D12_COMMAND_QUEUE_DESC キュー記述子 = {};
        キュー記述子.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        キュー記述子.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        winrt::check_hresult(デバイス_->CreateCommandQueue(&キュー記述子, 
            __uuidof(コマンドキュー_), コマンドキュー_.put_void()));

        // スワップチェーンを記述し、作成する。
        DXGI_SWAP_CHAIN_DESC1 スワップチェーン記述子 = {};
        スワップチェーン記述子.BufferCount = フレームバッファ数;
        スワップチェーン記述子.Width = 幅;
        スワップチェーン記述子.Height = 高さ;
        スワップチェーン記述子.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        スワップチェーン記述子.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        スワップチェーン記述子.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        スワップチェーン記述子.SampleDesc.Count = 1;

        winrt::com_ptr<IDXGISwapChain1> スワップチェーン;
        winrt::check_hresult(ファクトリー->CreateSwapChainForHwnd(
            コマンドキュー_.get(),// スワップチェーンは強制的なフラッシュにキューが必要
            ウィンドウハンドル,
            &スワップチェーン記述子,
            nullptr,
            nullptr,
            スワップチェーン.put()
        ));

        // このアプリケーションはフルスクリーンのトランジションをサポートしていない
        winrt::check_hresult(ファクトリー->MakeWindowAssociation(ウィンドウハンドル, 
            DXGI_MWA_NO_ALT_ENTER));

        スワップチェーン.as(スワップチェーン_);
        バックバッファ番号_ = スワップチェーン_->GetCurrentBackBufferIndex();

        // デスクリプターヒープを作成
        {
            // レンダー・ターゲット・ビュー（RTV）デスクリプターヒープを記述し、作成する。
            D3D12_DESCRIPTOR_HEAP_DESC rtvヒープ記述子 = {};
            rtvヒープ記述子.NumDescriptors = フレームバッファ数;
            rtvヒープ記述子.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtvヒープ記述子.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            winrt::check_hresult(デバイス_->CreateDescriptorHeap(&rtvヒープ記述子, 
                __uuidof(rtvヒープ_), rtvヒープ_.put_void()));

            // ■追加：深度ステンシルビュー（DSV）デスクリプターヒープを記述し、作成する。
            D3D12_DESCRIPTOR_HEAP_DESC dsvヒープ記述子 = {};
            dsvヒープ記述子.NumDescriptors = 1;
            dsvヒープ記述子.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            dsvヒープ記述子.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            winrt::check_hresult(デバイス_->CreateDescriptorHeap(&dsvヒープ記述子,
                __uuidof(dsvヒープ_), dsvヒープ_.put_void()));

            // オブジェクト用のシェーダー・リソース・ビュー(SRV)ヒープを記述し、作成する。
            D3D12_DESCRIPTOR_HEAP_DESC オブジェクトヒープ記述子 = {};
            オブジェクトヒープ記述子.NumDescriptors = 2;// ■変更：テクスチャと定数バッファ
            オブジェクトヒープ記述子.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            オブジェクトヒープ記述子.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            winrt::check_hresult(デバイス_->CreateDescriptorHeap(&オブジェクトヒープ記述子,
                __uuidof(オブジェクトヒープ_), オブジェクトヒープ_.put_void()));

            rtv記述子サイズ_ = デバイス_->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        }

        // フレームリソースを作成
        {
            CD3DX12_CPU_DESCRIPTOR_HANDLE rtvハンドル(
                rtvヒープ_->GetCPUDescriptorHandleForHeapStart());

            // フレームごとのRTVを作成
            for (UINT n = 0; n < フレームバッファ数; n++)
            {
                winrt::check_hresult(スワップチェーン_->GetBuffer(n, 
                    __uuidof(レンダーターゲット_[n]), レンダーターゲット_[n].put_void()));
                デバイス_->CreateRenderTargetView(レンダーターゲット_[n].get(), nullptr,
                    rtvハンドル);
                rtvハンドル.Offset(1, rtv記述子サイズ_);

                winrt::check_hresult(デバイス_->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT, 
                    __uuidof(コマンドアロケーター_[n]), コマンドアロケーター_[n].put_void()));
            }
        }
        // ■追加：深度バッファと深度ステンシルビューの生成
        {
            D3D12_CLEAR_VALUE 深度クリア値 = {};
            深度クリア値.Format = DXGI_FORMAT_D32_FLOAT;
            深度クリア値.DepthStencil.Depth = 1.0f;
            深度クリア値.DepthStencil.Stencil = 0;

			CD3DX12_HEAP_PROPERTIES 深度ヒーププロパティ(D3D12_HEAP_TYPE_DEFAULT);
			CD3DX12_RESOURCE_DESC 深度リソース記述子 =
                CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, 幅_, 高さ_, 1, 1);
            深度リソース記述子.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            winrt::check_hresult(デバイス_->CreateCommittedResource(
                &深度ヒーププロパティ, D3D12_HEAP_FLAG_NONE,
                &深度リソース記述子, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &深度クリア値,
                __uuidof(深度ステンシル_), 深度ステンシル_.put_void()
            ));
            深度ステンシル_->SetName(L"深度ステンシルリソース");

            D3D12_DEPTH_STENCIL_VIEW_DESC 深度ステンシル記述子 = {};
            深度ステンシル記述子.Format = DXGI_FORMAT_D32_FLOAT;
            深度ステンシル記述子.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            深度ステンシル記述子.Flags = D3D12_DSV_FLAG_NONE;
            デバイス_->CreateDepthStencilView(深度ステンシル_.get(), &深度ステンシル記述子,
                dsvヒープ_->GetCPUDescriptorHandleForHeapStart());
        }
    }

	// ランダムな色のテクスチャデータを生成
    static std::vector<uint32_t> テクスチャデータの生成(int 幅, int 高さ)
    {
        const int テクスチャサイズ = 幅 * 高さ;
		std::vector<uint32_t> データ(テクスチャサイズ);

        std::mt19937 乱数エンジン;
        std::random_device デバイス乱数;
        乱数エンジン.seed(デバイス乱数());
		std::uniform_int_distribution<uint32_t> 一様分布(0, 0xffffff);

        for (int n = 0; n < テクスチャサイズ; n++)
        {
            データ[n] = 0xff000000 + 一様分布(乱数エンジン);
        }

        return データ;
    }

	void Application::アセットの読み込み()
	{
        // ルート署名を作成する。
        {
            D3D12_FEATURE_DATA_ROOT_SIGNATURE 機能データ = {};

			// これは、サポートする最高バージョン。CheckFeatureSupportが成功すると、
            // 返されるHighestVersionはこれより大きくならない
            機能データ.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

            if (FAILED(デバイス_->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, 
                &機能データ, sizeof(機能データ))))
            {
                機能データ.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
            }

            CD3DX12_DESCRIPTOR_RANGE1 範囲配列[2];// ■修正
            範囲配列[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0,
                D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
            範囲配列[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0,
                D3D12_DESCRIPTOR_RANGE_FLAG_NONE);// ■追加

            CD3DX12_ROOT_PARAMETER1 ルートパラメータ配列[2];// ■修正
            ルートパラメータ配列[0].InitAsDescriptorTable(1,
                &範囲配列[0], D3D12_SHADER_VISIBILITY_PIXEL);
            ルートパラメータ配列[1].InitAsDescriptorTable(1,
                &範囲配列[1], D3D12_SHADER_VISIBILITY_VERTEX);// ■追加

            D3D12_STATIC_SAMPLER_DESC サンプラー記述子 = {};
            サンプラー記述子.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
            サンプラー記述子.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            サンプラー記述子.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            サンプラー記述子.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            サンプラー記述子.MipLODBias = 0;
            サンプラー記述子.MaxAnisotropy = 0;
            サンプラー記述子.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
            サンプラー記述子.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
            サンプラー記述子.MinLOD = 0.0f;
            サンプラー記述子.MaxLOD = D3D12_FLOAT32_MAX;
            サンプラー記述子.ShaderRegister = 0;
            サンプラー記述子.RegisterSpace = 0;
            サンプラー記述子.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC ルート署名記述子;
            ルート署名記述子.Init_1_1(_countof(ルートパラメータ配列), ルートパラメータ配列, 
                1, &サンプラー記述子, 
                D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

            winrt::com_ptr<ID3DBlob> 署名;
            winrt::com_ptr<ID3DBlob> エラー;
            winrt::check_hresult(D3DX12SerializeVersionedRootSignature(&ルート署名記述子,
                機能データ.HighestVersion, 署名.put(), エラー.put()));
            winrt::check_hresult(デバイス_->CreateRootSignature(0,
                署名->GetBufferPointer(), 署名->GetBufferSize(), 
                __uuidof(ルート署名_), ルート署名_.put_void()));
        }

        // シェーダーのコンパイルと読み込みを含む、パイプライン状態を作成する。
        {
            winrt::com_ptr<ID3DBlob> 頂点シェーダ, ピクセルシェーダ;
            winrt::com_ptr<ID3DBlob> エラー;

            UINT コンパイルフラグ = 0;
#if defined(_DEBUG)
            // グラフィックデバッグツールを使った、より優れたシェーダーデバッグを可能にする
            コンパイルフラグ |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
            HRESULT result;
            result = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", 
                コンパイルフラグ, 0, 頂点シェーダ.put(), エラー.put()); if (result) ShowError(result, エラー.get());
            result = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0",
                コンパイルフラグ, 0, ピクセルシェーダ.put(), エラー.put()); if (result) ShowError(result, エラー.get());

            // 頂点入力レイアウトを定義する。
            D3D12_INPUT_ELEMENT_DESC 入力要素記述子[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
            };

            // グラフィックスパイプラインステートオブジェクト（PSO）を記述し、作成する。
            D3D12_GRAPHICS_PIPELINE_STATE_DESC pso記述子 = {};
            pso記述子.InputLayout = { 入力要素記述子, _countof(入力要素記述子) };
            pso記述子.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;// 三角形リスト
            pso記述子.pRootSignature = ルート署名_.get();
            pso記述子.VS = CD3DX12_SHADER_BYTECODE(頂点シェーダ.get());
            pso記述子.PS = CD3DX12_SHADER_BYTECODE(ピクセルシェーダ.get());
            pso記述子.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);// ラスタライズ用の設定
			pso記述子.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);// 結果の合成用の設定
            pso記述子.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC();// ■修正：深度バッファを使う
			pso記述子.SampleMask = UINT_MAX;// サンプルマスクを使わない
			pso記述子.NumRenderTargets = 1;// レンダーターゲットの数
			pso記述子.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;// レンダーターゲットのフォーマット
			pso記述子.SampleDesc.Count = 1;// サンプリングの数
            winrt::check_hresult(デバイス_->CreateGraphicsPipelineState(&pso記述子,
                __uuidof(パイプラインステート_), パイプラインステート_.put_void()));
        }

        // コマンドリストを生成
        winrt::check_hresult(デバイス_->CreateCommandList(0, 
            D3D12_COMMAND_LIST_TYPE_DIRECT, 
            コマンドアロケーター_[バックバッファ番号_].get(), 
            パイプラインステート_.get(), 
            __uuidof(コマンドリスト_), コマンドリスト_.put_void()));

        // ■追加：シーンオブジェクトを作成
		DirectX::XMVECTOR カメラ位置_   = DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
        DirectX::XMVECTOR カメラ注視点_ = DirectX::XMVectorSet(0.0f, 0.0f,  0.0f, 0.0f);
        ビュー行列_ = DirectX::XMMatrixLookAtLH(カメラ位置_, カメラ注視点_, DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    	射影行列_ = DirectX::XMMatrixPerspectiveFovLH(45.0f / 180.0f * DirectX::XM_PI, アスペクト比_, 0.01f, 100.0f);

        // 頂点バッファを作成
        {
            // ■修正：三角形のジオメトリを定義する(y成分のアスペクト比の消去)
            頂点 頂点列[] =
			{   //    x       y      z       テクスチャ座標
                { { +0.0f,  +0.25f, 0.0f }, { 0.5f, 0.0f }  },// 上
				{ { +0.25f, -0.25f, 0.0f }, { 1.0f, 1.0f }  },// 右下
				{ { -0.25f, -0.25f, 0.0f }, { 0.0f, 1.0f }  } // 左下
            };

            const UINT 頂点バッファサイズ = sizeof(頂点列);

            // 注: アップロード ヒープを使用して頂点バッファーのような静的データを転送するのはお勧めできない
            // GPU が必要とするたびに、アップロード ヒープが集結される。デフォルトのヒープ使用法で読んで欲しい。
            // コードを簡素化するため、また実際に転送する頂点が非常に少ないので、
            // ここではアップロード ヒープが使用されています。
			CD3DX12_RESOURCE_DESC 頂点バッファ記述子 = CD3DX12_RESOURCE_DESC::Buffer(頂点バッファサイズ);
			CD3DX12_HEAP_PROPERTIES ヒーププロパティ(D3D12_HEAP_TYPE_UPLOAD);
            winrt::check_hresult(デバイス_->CreateCommittedResource(
                &ヒーププロパティ,
                D3D12_HEAP_FLAG_NONE,
                &頂点バッファ記述子,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                __uuidof(頂点バッファ_), 頂点バッファ_.put_void()));

            // 三角形データを頂点バッファにコピー
            UINT8* 開始アドレス;
            CD3DX12_RANGE 読み込み範囲(0, 0); // このリソースをCPUから読む気はない
            winrt::check_hresult(頂点バッファ_->Map(0, &読み込み範囲, reinterpret_cast<void**>(&開始アドレス)));
            memcpy(開始アドレス, 頂点列, sizeof(頂点列));
            頂点バッファ_->Unmap(0, nullptr);

            // 頂点バッファビューの初期化
            頂点バッファビュー_.BufferLocation = 頂点バッファ_->GetGPUVirtualAddress();
            頂点バッファビュー_.StrideInBytes = sizeof(頂点);
            頂点バッファビュー_.SizeInBytes = 頂点バッファサイズ;
        }

        // 注意：ComPtrはCPUオブジェクトだが、このリソースは、それを参照するコマンドリストが
        // GPU上で実行を終了するまで、スコープ内にとどまる必要がある。リソースが早期に破壊
        // されないように、このメソッドの最後でGPUをフラッシュする。
        winrt::com_ptr<ID3D12Resource> テクスチャアップロードヒープ;

        // ■追加
        D3D12_CPU_DESCRIPTOR_HANDLE オブジェクトヒープハンドル = オブジェクトヒープ_->GetCPUDescriptorHandleForHeapStart();
        UINT srvヒープサイズ = デバイス_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // テクスチャの生成
        {
            int 幅 = 256;
            int 高さ = 256;
            int テクスチャ画素サイズ = 4;// RGBA

            // Texture2Dの記述と生成
            D3D12_RESOURCE_DESC テクスチャ記述子 = {};
            テクスチャ記述子.MipLevels = 1;
            テクスチャ記述子.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            テクスチャ記述子.Width = 幅;
            テクスチャ記述子.Height = 高さ;
            テクスチャ記述子.Flags = D3D12_RESOURCE_FLAG_NONE;
            テクスチャ記述子.DepthOrArraySize = 1;
            テクスチャ記述子.SampleDesc.Count = 1;
            テクスチャ記述子.SampleDesc.Quality = 0;
            テクスチャ記述子.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

            CD3DX12_HEAP_PROPERTIES ヒーププロパティ(D3D12_HEAP_TYPE_DEFAULT);
            winrt::check_hresult(デバイス_->CreateCommittedResource(
                &ヒーププロパティ,
                D3D12_HEAP_FLAG_NONE,
                &テクスチャ記述子,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                __uuidof(テクスチャ_), テクスチャ_.put_void()));

            const UINT64 アップロードバッファサイズ = GetRequiredIntermediateSize(
                テクスチャ_.get(), 0, 1);

            // GPU アップロードバッファの生成
            CD3DX12_HEAP_PROPERTIES アップロードヒーププロパティ(D3D12_HEAP_TYPE_UPLOAD);
            CD3DX12_RESOURCE_DESC アップロードバッファ記述子 =
                CD3DX12_RESOURCE_DESC::Buffer(アップロードバッファサイズ);
            winrt::check_hresult(デバイス_->CreateCommittedResource(
                &アップロードヒーププロパティ,
                D3D12_HEAP_FLAG_NONE,
                &アップロードバッファ記述子,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                __uuidof(テクスチャアップロードヒープ), テクスチャアップロードヒープ.put_void()));

            // 中間的なアップロードヒープにデータをコピーし、次にテクスチャ2Dへのコピーをスケジュール
            std::vector<uint32_t> 生テクスチャデータ = テクスチャデータの生成(幅, 高さ);

            D3D12_SUBRESOURCE_DATA テクスチャデータ = {};
            テクスチャデータ.pData = &生テクスチャデータ[0];
            テクスチャデータ.RowPitch = 幅 * テクスチャ画素サイズ;
            テクスチャデータ.SlicePitch = テクスチャデータ.RowPitch * 高さ;

            CD3DX12_RESOURCE_BARRIER テクスチャバリア = CD3DX12_RESOURCE_BARRIER::Transition(
                テクスチャ_.get(), D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            UpdateSubresources(コマンドリスト_.get(),
                テクスチャ_.get(), テクスチャアップロードヒープ.get(), 0, 0, 1, &テクスチャデータ);
            コマンドリスト_->ResourceBarrier(1, &テクスチャバリア);

            // テクスチャ用のSRVを記述して生成
            D3D12_SHADER_RESOURCE_VIEW_DESC srv記述子 = {};
            srv記述子.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv記述子.Format = テクスチャ記述子.Format;
            srv記述子.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv記述子.Texture2D.MipLevels = 1;
            デバイス_->CreateShaderResourceView(テクスチャ_.get(), &srv記述子, オブジェクトヒープハンドル);// ■修正
        }

        // ■追加：定数バッファの生成
        {
            const UINT 定数バッファデータサイズ = sizeof(オブジェクト定数バッファ);

			CD3DX12_HEAP_PROPERTIES ヒーププロパティ(D3D12_HEAP_TYPE_UPLOAD);
			CD3DX12_RESOURCE_DESC リソース記述子 = CD3DX12_RESOURCE_DESC::Buffer(定数バッファデータサイズ);
            winrt::check_hresult(デバイス_->CreateCommittedResource(
                &ヒーププロパティ, D3D12_HEAP_FLAG_NONE,
                &リソース記述子, D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                __uuidof(定数バッファ_), 定数バッファ_.put_void()));
			定数バッファ_->SetName(L"定数バッファ");

            // 定数バッファをマッピングし、初期化する。アプリが終了するまで、このマッピングは解除しない。
            // リソースの寿命が尽きるまでマッピングされたままにしておいても問題ない。
            CD3DX12_RANGE readRange(0, 0);        // CPU上でこのリソースから読み出すつもりはない
            winrt::check_hresult(定数バッファ_->Map(0, &readRange, 
                reinterpret_cast<void**>(&マップされたオブジェクト定数バッファ_)));

            // 定数バッファの初期化
            オブジェクト定数バッファ 初期化データ;
            DirectX::XMStoreFloat4x4(&(初期化データ.変換行列), XMMatrixTranspose(ビュー行列_* 射影行列_));
            memcpy(マップされたオブジェクト定数バッファ_, &初期化データ, 定数バッファデータサイズ);

            // 定数バッファビューの作成
            オブジェクトヒープハンドル.ptr += srvヒープサイズ;// テクスチャのSRVの次の位置に置く
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbv記述子 = {};
            cbv記述子.BufferLocation = 定数バッファ_->GetGPUVirtualAddress();
            cbv記述子.SizeInBytes = static_cast<UINT>(定数バッファ_->GetDesc().Width);
            デバイス_->CreateConstantBufferView(&cbv記述子, オブジェクトヒープハンドル);
        }

        // 最初のGPUセットアップを始めるためにコマンドリストを閉じて実行する
        winrt::check_hresult(コマンドリスト_->Close());
        ID3D12CommandList* コマンドリスト列[] = { コマンドリスト_.get() };
        コマンドキュー_->ExecuteCommandLists(_countof(コマンドリスト列), コマンドリスト列);

        // 同期オブジェクトを作成し、アセットがGPUにアップロードされるまで待つ。
        {
            winrt::check_hresult(デバイス_->CreateFence(
                フェンス値_[バックバッファ番号_], D3D12_FENCE_FLAG_NONE, 
                __uuidof(フェンス_), フェンス_.put_void()));
            フェンス値_[バックバッファ番号_]++;

            // フレーム同期に使用するイベントハンドルを作成する
            フェンスイベント_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (フェンスイベント_ == nullptr)
            {
                winrt::check_hresult(HRESULT_FROM_WIN32(GetLastError()));
            }

            // コマンドリストの実行を待つ。メインループで同じコマンドリストを再利用して
            // おり、今はセットアップが完了するのを待ってから続けたい。
            GPUを待つ();
        }
    }


    void Application::コマンドリストの記録()
    {
       // コマンド・リスト・アロケータは、関連するコマンド・リストがGPUでの
       // 実行を終了したときにのみリセットすることができる。
       // アプリはGPU実行の進捗の判断にフェンスを使うべき。
        winrt::check_hresult(コマンドアロケーター_[バックバッファ番号_]->Reset());

        // コマンド・リストでExecuteCommandList()を呼び出すと、
        // そのコマンド・リストはいつでもリセットすることができるが、
        // 再記録の前にリセットしなければならない。
        winrt::check_hresult(コマンドリスト_->Reset(
            コマンドアロケーター_[バックバッファ番号_].get(), 
            パイプラインステート_.get()));

        // バックバッファをレンダーターゲットとして使用する
        ID3D12Resource* レンダーターゲット = レンダーターゲット_[バックバッファ番号_].get();
        CD3DX12_RESOURCE_BARRIER バリアPresent2RT(CD3DX12_RESOURCE_BARRIER::Transition(
            レンダーターゲット,
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
        コマンドリスト_->ResourceBarrier(1, &バリアPresent2RT);

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvハンドル(
            rtvヒープ_->GetCPUDescriptorHandleForHeapStart(), バックバッファ番号_, rtv記述子サイズ_);
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvハンドル(
            dsvヒープ_->GetCPUDescriptorHandleForHeapStart());// ■追加
        コマンドリスト_->OMSetRenderTargets(1, &rtvハンドル, FALSE, &dsvハンドル);// ■修正

        // コマンドを記録
        const float 背景色[] = { 0.0f, 0.2f, 0.4f, 1.0f };// 赤、緑、青、アルファ
        コマンドリスト_->ClearRenderTargetView(rtvハンドル, 背景色, 0, nullptr);
        コマンドリスト_->ClearDepthStencilView(dsvハンドル, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);// ■追加

        // 描画に必要な状態を設定
        コマンドリスト_->SetGraphicsRootSignature(ルート署名_.get());
        コマンドリスト_->RSSetViewports(1, &ビューポート_);
        コマンドリスト_->RSSetScissorRects(1, &はさみ矩形_);

		// ■修正：テクスチャや定数バッファをバインド
        ID3D12DescriptorHeap* ppHeaps[] = { オブジェクトヒープ_.get() };
        auto ヒープハンドル = オブジェクトヒープ_->GetGPUDescriptorHandleForHeapStart();
        コマンドリスト_->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
        コマンドリスト_->SetGraphicsRootDescriptorTable(0, ヒープハンドル);
        ヒープハンドル.ptr += デバイス_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        コマンドリスト_->SetGraphicsRootDescriptorTable(1, ヒープハンドル);

        // ■修正：ポリゴンの描画
        DirectX::XMMATRIX ワールド行列 = DirectX::XMMatrixRotationY(angle);
        DirectX::XMMATRIX 変換行列 = ワールド行列 * ビュー行列_ * 射影行列_;
        // シェーダは列優先として行列を作用させるがDirectXMathは行優先を想定して計算するので転置して渡す
        変換行列 = XMMatrixTranspose(変換行列);
        DirectX::XMStoreFloat4x4(&(マップされたオブジェクト定数バッファ_->変換行列), 変換行列);

        コマンドリスト_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        コマンドリスト_->IASetVertexBuffers(0, 1, &頂点バッファビュー_);
		コマンドリスト_->DrawInstanced(3, 1, 0, 0);// 3頂点、1インスタンス

        // バックバッファは画面更新に使用される
		CD3DX12_RESOURCE_BARRIER バリアRT2Present(CD3DX12_RESOURCE_BARRIER::Transition(
            レンダーターゲット,
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        コマンドリスト_->ResourceBarrier(1, &バリアRT2Present);

        winrt::check_hresult(コマンドリスト_->Close());
    }

    void Application::GPUを待つ()
    {
        // キューにシグナルコマンドをスケジュールする
        winrt::check_hresult(コマンドキュー_->Signal(フェンス_.get(), 
            フェンス値_[バックバッファ番号_]));

        // フェンスの処理が終わるまで待つ
        winrt::check_hresult(フェンス_->SetEventOnCompletion(
            フェンス値_[バックバッファ番号_], フェンスイベント_));
        WaitForSingleObjectEx(フェンスイベント_, INFINITE, FALSE);

        // 現在のフレームのフェンス値を増加させる
        フェンス値_[バックバッファ番号_]++;
    }

    // 次のフレームのレンダリングを準備
    void Application::次のフレームに行く()
    {
        // キューにシグナルコマンドをスケジュールする
        const UINT64 現在のフェンス値 = フェンス値_[バックバッファ番号_];
        winrt::check_hresult(コマンドキュー_->Signal(フェンス_.get(), 現在のフェンス値));

        // フレームインデックスを更新
        バックバッファ番号_ = スワップチェーン_->GetCurrentBackBufferIndex();

        // 次のフレームをレンダリングする準備がまだできていない場合は、
        // 準備ができるまで待つ。
        if (フェンス_->GetCompletedValue() < フェンス値_[バックバッファ番号_])
        {
            winrt::check_hresult(フェンス_->SetEventOnCompletion(
                フェンス値_[バックバッファ番号_], フェンスイベント_));
            WaitForSingleObjectEx(フェンスイベント_, INFINITE, FALSE);
        }

        // 次のフレームのフェンス値を設定する
        フェンス値_[バックバッファ番号_] = 現在のフェンス値 + 1;
    }

    void Application::ハードウェアアダプターの取得(
        _In_ IDXGIFactory1* pファクトリー,
        _Outptr_result_maybenull_ IDXGIAdapter1** ppアダプター,
        bool 高パフォーマンスアダプターの要求)
    {
        *ppアダプター = nullptr;

        winrt::com_ptr<IDXGIAdapter1> アダプター;

        winrt::com_ptr<IDXGIFactory6> ファクトリー6;
        if (winrt::check_hresult(pファクトリー->QueryInterface(
            __uuidof(ファクトリー6), ファクトリー6.put_void())))
        {
            for (
                UINT アダプター番号 = 0;
                winrt::check_hresult(ファクトリー6->EnumAdapterByGpuPreference(
                    アダプター番号,
                    高パフォーマンスアダプターの要求 == true ? 
                        DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : 
                        DXGI_GPU_PREFERENCE_UNSPECIFIED,
                    __uuidof(アダプター), アダプター.put_void()));
                    ++アダプター番号)
            {
                DXGI_ADAPTER_DESC1 記述子;
                アダプター->GetDesc1(&記述子);

                if (記述子.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Basic Render Driverアダプタは選択しないでください。
                    // ソフトウェア・アダプタが必要な場合は、コマンドラインに「/warp 」
                    // と入力してください。
                    continue;
                }

                // アダプタがDirect3D 12をサポートしているかどうかを確認するが、
                // 実際のデバイスはまだ作成しない
                if (winrt::check_hresult(D3D12CreateDevice(アダプター.get(),
                    D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
                {
                    break;
                }
            }
        }

        if (アダプター.get() == nullptr)
        {
            for (UINT アダプター番号 = 0; winrt::check_hresult(
                pファクトリー->EnumAdapters1(アダプター番号, アダプター.put()));
                ++アダプター番号)
            {
                DXGI_ADAPTER_DESC1 記述子;
                アダプター->GetDesc1(&記述子);

                if (記述子.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Basic Render Driverアダプタは選択しないでください。
                    // ソフトウェア・アダプタが必要な場合は、コマンドラインに「/warp 」
                    // と入力してください。
                    continue;
                }

                // アダプタがDirect3D 12をサポートしているかどうかを確認するが、
                // 実際のデバイスはまだ作成しない
                if (winrt::check_hresult(D3D12CreateDevice(アダプター.get(),
                    D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
                {
                    break;
                }
            }
        }

        *ppアダプター = アダプター.detach();
    }

    void Application::ShowError(HRESULT result, ID3DBlob* error)
    {
        if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
            ::OutputDebugString(L"ファイルが見当たりません\n");
            exit(1);
        }

        std::string s((char*)error->GetBufferPointer(), error->GetBufferSize());
        s += "\n";
        OutputDebugStringA(s.c_str());

        exit(1);
    }
}
