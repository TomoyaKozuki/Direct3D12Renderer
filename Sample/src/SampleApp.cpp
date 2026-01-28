//-----------------------------------------------------------------------------
// File : SampleApp.cpp
// Desc : Sample Application Module.
// Copyright(c) Pocol. All right reserved.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "SampleApp.h"
#include "FileUtil.h"
#include "Logger.h"
#include "CommonStates.h"
#include "DirectXHelpers.h"
#include "SimpleMath.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include "ImguiUtil.h"
#include <iostream>
#include <EnumUtil.h>
#include <windows.h>
#include <shellapi.h>
#include <tchar.h>
#include <iostream>
#include <array>
#include <Winuser.h>
#include <windowsx.h>


#include "../extern/nv_helpers_dx12/include/BottomLevelASGenerator.h"
#include "../extern/nv_helpers_dx12/include/RaytracingPipelineGenerator.h"
#include "../extern/nv_helpers_dx12/include/RootSignatureGenerator.h"
#include "../extern/nv_helpers_dx12/include/DXRHelper.h"


#pragma comment( lib, "dxcompiler.lib" )
//-----------------------------------------------------------------------------
// Using Statements
//-----------------------------------------------------------------------------
using namespace DirectX::SimpleMath;

namespace {

///////////////////////////////////////////////////////////////////////////////
// Transform structure
///////////////////////////////////////////////////////////////////////////////
struct Transform
{
    Matrix   World;      //!< ワールド行列です.
    Matrix   View;       //!< ビュー行列です.
    Matrix   Proj;       //!< 射影行列です.
    Matrix   InvView;    //!< ビュー行列の逆行列です.
    Matrix   InvProj;    //!< 射影行列の逆行列です.
};

///////////////////////////////////////////////////////////////////////////////
// LightBuffer structure
///////////////////////////////////////////////////////////////////////////////
struct LightBuffer
{
    Vector4  LightPosition;     //!< ライト位置です.
    Color    LightColor;        //!< ライトカラーです.
    Vector4  CameraPosition;    //!< カメラ位置です.
};

///////////////////////////////////////////////////////////////////////////////
// MaterialBuffer structure
///////////////////////////////////////////////////////////////////////////////
struct MaterialBuffer
{
    Vector3 BaseColor;  //!< 基本色.
    float   Alpha;      //!< 透過度.
    float   Roughness;  //!< 面の粗さです(範囲は[0,1]).
    float   Metallic;   //!< 金属度です(範囲は[0,1]).
};

} // namespace

DWORD CALLBACK MyReadProc(DWORD_PTR dwCookie, LPBYTE pbBuf, LONG cb, LONG* pcb);

//-----------------------------------------------------------------------------
//      テクスチャセットを設定します.
//-----------------------------------------------------------------------------
void SetTextureSet(
    const std::wstring& base_path,
    Material& material,
    DirectX::ResourceUploadBatch& batch
)
{
    std::wstring pathBC = base_path + L"basecolor.dds";
    std::wstring pathM = base_path + L"metallic.dds";
    std::wstring pathR = base_path + L"roughness.dds";
    std::wstring pathN = base_path + L"normal.dds";

    material.SetTexture(0, TU_BASE_COLOR, pathBC, batch);
    material.SetTexture(0, TU_METALLIC, pathM, batch);
    material.SetTexture(0, TU_ROUGHNESS, pathR, batch);
    material.SetTexture(0, TU_NORMAL, pathN, batch);
}

///////////////////////////////////////////////////////////////////////////////
// SampleApp class
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//      コンストラクタです.
//-----------------------------------------------------------------------------
SampleApp::SampleApp(uint32_t width, uint32_t height)
: App(width, height)
, m_RotateAngle(0.0)
{ /* DO_NOTHING */ }

//-----------------------------------------------------------------------------
//      デストラクタです.
//-----------------------------------------------------------------------------
SampleApp::~SampleApp()
{ /* DO_NOTHING */ }

//-----------------------------------------------------------------------------
//      DXR初期化時の処理です.
//-----------------------------------------------------------------------------
bool SampleApp::OnDXRInit() {
    //デバイスがレイトレーシングの機能を有しているかチェック
    
    CheckRaytracingSupport();
    std::cout << "CheckRaytracingSupport " << std::endl;

    m_CommandList.Reset();//しまっていたので、追加した。
    //レイトレーシングの加速構造ASを構築。構築時には、加速構造を作るときにはBLASに固有のTransform行列を持つ
    CreateAccelerationStructures();
    std::cout << "CreateAccelerationStructures" << std::endl;
    //コマンドリストは生成直後は「記録中」であるが、まだ何も記録しないので「close状態」にしておく
    //ThrowIfFailed(m_CommandList.GetCommandList()->Close());
    //シェーダコード ↔ シンボル名の対応 シンボル名 ↔ ルートシグニチャの対応 レイが運ぶデータ量（Ray Payload）

    // CreateAccelerationStructuresでコマンドを記録して、いったん閉じて、CreateRaytracingPipelineに進むのはなぜ？
    //理由は「AS（Acceleration Structure：加速構造体）の構築コマンドをGPUに確実に実行させて完了させてから、
    // レイトレーシングパイプライン（CreateRaytracingPipeline）の初期化に進む必要がある」ためです。
    CreateRaytracingPipeline();
    std::cout << "CreateRaytracingPipeline" << std::endl;
    // #DXR Extra: Per-Instance Data
    CreatePerInstanceConstantBuffers();
    std::cout << "CreatePerInstanceConstantBuffers" << std::endl;
    // #DXR Extra: Per-Instance Data
    // Create a constant buffers, with a color for each vertex of the triangle,
    // for each triangle instance
    CreateGlobalConstantBuffer();
    std::cout << "CreateGlobalConstantBuffer " << std::endl;
    CreateRaytracingOutputBuffer(); // #DXR
    std::cout << "CreateRaytracingOutputBuffer " << std::endl;
    // DXR における「インスタンスごとの情報を保持するバッファを作る処理」
    // 各インスタンス（オブジェクト）ごとの
    // 変換行列（objectToWorldなど）を格納するためのバッファ（m_instanceProperties）を作成する関数
    CreateInstancePropertiesBuffer();
    std::cout << "CreateInstancePropertiesBuffer " << std::endl;
    //カメラバッファの作成
    CreateCameraBuffer();
    std::cout << "CreateCameraBuffer " << std::endl;
    //レイトレーシング結果用バッファ（UAV） と　レイトレーシング用ディスクリプタヒープ　を作成
    //Raytracing では：RayGen Miss ClosestHit　すべてのシェーダが： 同じディスクリプタヒープを見る（＝ Global Root Signature）
    CreateShaderResourceHeap();
    std::cout << "CreateShaderResourceHeap " << std::endl;

    //レイトレーシングの出力結果を保存するバッファを確保する。サイズは最終的に表示する画像（画面・バックバッファ）と同じ。
    CreateShaderBindingTable();
    std::cout << "CreateShaderBindingTable " << std::endl;

    // --- ここを追加 ---
    // 1. ノートを閉じる
    m_CommandList.GetCommandList()->Close();
    
    // 2. 溜まったコマンド（SBTの構築やコピーなど）をGPUに送る
    ID3D12CommandList* ppCommandLists[] = { m_CommandList.GetCommandList() };
    m_pQueue->ExecuteCommandLists(1, ppCommandLists);
    
    // 3. 初期化コマンドがすべて終わるのを待つ（これ重要！）
    m_Fence.Sync(m_pQueue.Get()); 
    // ------------------

    return true;
}
//-----------------------------------------------------------------------------
//      初期化時の処理です.
//-----------------------------------------------------------------------------
bool SampleApp::OnInit()
{
    // メッシュをロード.
    {
        std::wstring path;

        // ファイルパスを検索.
        if (!SearchFilePath(L"../../../Sample/res/buster_sword/sword.obj", path))
        {
            ELOG("Error : File Not Found.");
            return false;
        }

        std::wstring dir = GetDirectoryPath(path.c_str());

        std::vector<ResMesh>        resMesh;
        std::vector<ResMaterial>    resMaterial;
        
        // メッシュリソースをロード.
        if (!LoadMesh(path.c_str(), resMesh, resMaterial))
        {
            ELOG("Error : Load Mesh Failed. filepath = %ls", path.c_str());
            return false;
        }

        // メモリを予約.
        m_pMesh.reserve(resMesh.size());

        // メッシュを初期化.
        for (size_t i = 0; i < resMesh.size(); ++i)
        {
            // メッシュ生成.
            auto mesh = new (std::nothrow) Mesh();

            // チェック.
            if (mesh == nullptr)
            {
                ELOG( "Error : Out of memory.");
                return false;
            }

            // 初期化処理.
            if (!mesh->Init(m_pDevice.Get(), resMesh[i]))
            {
                ELOG( "Error : Mesh Initialize Failed.");
                delete mesh;
                return false;
            }

            // 成功したら登録.
            m_pMesh.push_back(mesh);
        }

        // メモリ最適化.
        m_pMesh.shrink_to_fit();

        // マテリアル初期化.
        if (!m_Material.Init(
            m_pDevice.Get(),
            m_pPool[POOL_TYPE_RES],
            sizeof(MaterialBuffer),
            resMaterial.size()))
        {
            ELOG( "Error : Material::Init() Failed.");
            return false;
        }

        // リソースバッチを用意.
        DirectX::ResourceUploadBatch batch(m_pDevice.Get());

        // バッチ開始.
        batch.Begin();

        SetTextureSet(L"../../../Sample/res/buster_sword/", m_Material, batch);
        // バッチ終了.
        auto future = batch.End(m_pQueue.Get());

        // バッチ完了を待機.
        future.wait();
    }

    // ライトバッファの設定.
    {
        auto pCB = new (std::nothrow) ConstantBuffer();
        if (pCB == nullptr)
        {
            ELOG( "Error : Out of memory." );
            return false;
        }

        if (!pCB->Init(m_pDevice.Get(), m_pPool[POOL_TYPE_RES], sizeof(LightBuffer)))
        {
            ELOG( "Error : ConstantBuffer::Init() Failed." );
            return false;
        }

        auto ptr = pCB->GetPtr<LightBuffer>();
        
        ptr->LightPosition  = Vector4(0.0f, -100.0f, 1500.0f, 0.0);//Vector4(m_eyePos.x, m_eyePos.y, m_eyePos.z + 10000.0f, 0.0f);
        ptr->LightColor     = Color(1.0f,  1.0f, 1.0f, m_LightIntensity);
        m_eyePos = Vector3(0.0f, 0.0f, 3.0f);
        ptr->CameraPosition = Vector4(m_eyePos.x, m_eyePos.y, m_eyePos.z, 0.0f);//ptr->CameraPosition = Vector4(0.0f, 0.0f, 3.0f, 0.0f);
        m_pLight = pCB;
    }

    // ルートシグニチャの生成.
    {
        auto flag = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
        flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
        flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        // ディスクリプタレンジを設定.
        D3D12_DESCRIPTOR_RANGE range[4] = {};
        range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range[0].NumDescriptors = 1;
        range[0].BaseShaderRegister = 0;
        range[0].RegisterSpace = 0;
        range[0].OffsetInDescriptorsFromTableStart = 0;

        range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range[1].NumDescriptors = 1;
        range[1].BaseShaderRegister = 1;
        range[1].RegisterSpace = 0;
        range[1].OffsetInDescriptorsFromTableStart = 0;

        range[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range[2].NumDescriptors = 1;
        range[2].BaseShaderRegister = 2;
        range[2].RegisterSpace = 0;
        range[2].OffsetInDescriptorsFromTableStart = 0;

        range[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range[3].NumDescriptors = 1;
        range[3].BaseShaderRegister = 3;
        range[3].RegisterSpace = 0;
        range[3].OffsetInDescriptorsFromTableStart = 0;

        // ルートパラメータの設定.
        D3D12_ROOT_PARAMETER param[7] = {};
        param[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
        param[0].Descriptor.ShaderRegister = 0;
        param[0].Descriptor.RegisterSpace  = 0;
        param[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_VERTEX;

        param[1].ParameterType              = D3D12_ROOT_PARAMETER_TYPE_CBV;
        param[1].Descriptor.ShaderRegister  = 1;
        param[1].Descriptor.RegisterSpace   = 0;
        param[1].ShaderVisibility           = D3D12_SHADER_VISIBILITY_PIXEL;

        param[2].ParameterType              = D3D12_ROOT_PARAMETER_TYPE_CBV;
        param[2].Descriptor.ShaderRegister  = 2;
        param[2].Descriptor.RegisterSpace   = 0;
        param[2].ShaderVisibility           = D3D12_SHADER_VISIBILITY_PIXEL;

        param[3].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param[3].DescriptorTable.NumDescriptorRanges = 1;
        param[3].DescriptorTable.pDescriptorRanges   = &range[0];
        param[3].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

        param[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param[4].DescriptorTable.NumDescriptorRanges = 1;
        param[4].DescriptorTable.pDescriptorRanges = &range[1];
        param[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        param[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param[5].DescriptorTable.NumDescriptorRanges = 1;
        param[5].DescriptorTable.pDescriptorRanges = &range[2];
        param[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        param[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param[6].DescriptorTable.NumDescriptorRanges = 1;
        param[6].DescriptorTable.pDescriptorRanges = &range[3];
        param[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        // スタティックサンプラーの設定.
        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter              = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU            = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressV            = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressW            = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.MipLODBias          = D3D12_DEFAULT_MIP_LOD_BIAS;
        sampler.MaxAnisotropy       = 1;
        sampler.ComparisonFunc      = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor         = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD              = -D3D12_FLOAT32_MAX;
        sampler.MaxLOD              = +D3D12_FLOAT32_MAX;
        sampler.ShaderRegister      = 0;
        sampler.RegisterSpace       = 0;
        sampler.ShaderVisibility    = D3D12_SHADER_VISIBILITY_PIXEL;

        // ルートシグニチャの設定.
        D3D12_ROOT_SIGNATURE_DESC desc = {};
        desc.NumParameters      = _countof(param);
        desc.NumStaticSamplers  = 1;
        desc.pParameters        = param;
        desc.pStaticSamplers    = &sampler;
        desc.Flags              = flag;

        ComPtr<ID3DBlob> pBlob;
        ComPtr<ID3DBlob> pErrorBlob;

        // シリアライズ
        auto hr = D3D12SerializeRootSignature(
            &desc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            pBlob.GetAddressOf(),
            pErrorBlob.GetAddressOf());
        if ( FAILED(hr) )
        { return false; }

        // ルートシグニチャを生成.
        hr = m_pDevice->CreateRootSignature(
            0,
            pBlob->GetBufferPointer(),
            pBlob->GetBufferSize(),
            IID_PPV_ARGS(m_pRootSig.GetAddressOf()));
        if ( FAILED(hr) )
        {
            ELOG( "Error : Root Signature Create Failed. retcode = 0x%x", hr );
            return false;
        }
    }

    // パイプラインステートの生成.
    {
        std::wstring vsPath;
        std::wstring psPath;

        // 頂点シェーダを検索.
        if (!SearchFilePath(L"GGXVS.cso", vsPath))
        {
            ELOG( "Error : Vertex Shader Not Found.");
            return false;
        }

        // ピクセルシェーダを検索.
        if (!SearchFilePath(L"GGXPS.cso", psPath))
        {
            ELOG( "Error : Pixel Shader Node Found.");
            return false;
        }

        ComPtr<ID3DBlob> pVSBlob;
        ComPtr<ID3DBlob> pPSBlob;

        // 頂点シェーダを読み込む.
        auto hr = D3DReadFileToBlob( vsPath.c_str(), pVSBlob.GetAddressOf() );
        if ( FAILED(hr) )
        {
            ELOG( "Error : D3DReadFiledToBlob() Failed. path = %ls", vsPath.c_str() );
            return false;
        }

        // ピクセルシェーダを読み込む.
        hr = D3DReadFileToBlob( psPath.c_str(), pPSBlob.GetAddressOf() );
        if ( FAILED(hr) )
        {
            ELOG( "Error : D3DReadFileToBlob() Failed. path = %ls", psPath.c_str() );
            return false;
        }

        // グラフィックスパイプラインステートを設定.
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
        desc.InputLayout            = MeshVertex::InputLayout;
        desc.pRootSignature         = m_pRootSig.Get();
        desc.VS                     = { pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize() };
        desc.PS                     = { pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize() };
        desc.RasterizerState        = DirectX::CommonStates::CullNone;
        desc.BlendState             = DirectX::CommonStates::Opaque;
        desc.DepthStencilState      = DirectX::CommonStates::DepthDefault;
        desc.SampleMask             = UINT_MAX;
        desc.PrimitiveTopologyType  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets       = 1;
        desc.RTVFormats[0]          = m_ColorTarget[0].GetViewDesc().Format;
        //desc.RTVFormats[0]          = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        desc.DSVFormat              = m_DepthTarget.GetViewDesc().Format;
        desc.SampleDesc.Count       = 1;//本来は1
        desc.SampleDesc.Quality     = 0;

        // パイプラインステートを生成.
        hr = m_pDevice->CreateGraphicsPipelineState( &desc, IID_PPV_ARGS(m_pPSO.GetAddressOf()) );
        if ( FAILED(hr) )
        {
            ELOG( "Error : ID3D12Device::CreateGraphicsPipelineState() Failed. retcode = 0x%x", hr );
            return false;
        }
    }

    // 変換行列用の定数バッファの生成.
    {
        m_Transform.reserve(FrameCount);

        for(auto i=0u; i<FrameCount; ++i)
        {
            auto pCB = new (std::nothrow) ConstantBuffer();
            if (pCB == nullptr)
            {
                ELOG( "Error : Out of memory." );
                return false;
            }

            // 定数バッファ初期化.
            if (!pCB->Init(m_pDevice.Get(), m_pPool[POOL_TYPE_RES], sizeof(Transform) * 2))
            {
                ELOG( "Error : ConstantBuffer::Init() Failed." );
                return false;
            }

            // カメラ設定.
            /*auto eyePos     = Vector3( 0.0f, 0.0f, 3.0f );
            auto targetPos  = Vector3::Zero;
            auto upward     = Vector3::UnitY;*/

            //m_eyePos = Vector3(0.0f, 0.0f, 3.0f); 
            m_targetPos = Vector3(0.0f, 0.0f, 0.0f); 
            m_upward = Vector3(0.0f, 1.0f, 0.0f); 

            // 垂直画角とアスペクト比の設定.
            auto fovY   = DirectX::XMConvertToRadians( 37.5f );
            auto aspect = static_cast<float>(m_Width) / static_cast<float>(m_Height);

            // 変換行列を設定.
            auto ptr = pCB->GetPtr<Transform>();
            ptr->World = Matrix::Identity;
            //ptr->World = Matrix::CreateRotationY(0.3f);
            ptr->View  = Matrix::CreateLookAt( m_eyePos, m_targetPos, m_upward );
            ptr->Proj  = Matrix::CreatePerspectiveFieldOfView( fovY, aspect, 1.0f, 1000.0f );
            ptr->InvView = ptr->View;
            ptr->InvView.Invert();
            ptr->InvProj = ptr->Proj;
            ptr->InvProj.Invert();

            ptr->InvView = ptr->InvView.Transpose();
            ptr->InvProj = ptr->InvProj.Transpose();

            m_Transform.push_back(pCB);
        }

        m_RotateAngle = DirectX::XMConvertToRadians(0.0f);
    }

    m_ImGuiUtil.Initialize(m_hWnd, m_pDevice.Get());
    m_WindowEvent.emplace(GetHWND());

    std::cout << "CreatePlaneVB()" << std::endl;

    CreatePlaneVB();

    return true;
}

//-----------------------------------------------------------------------------
//      終了時の処理です.
//-----------------------------------------------------------------------------
void SampleApp::OnTerm()
{
    m_ImGuiUtil.Finalize();
    // メッシュ破棄.
    for(size_t i=0; i<m_pMesh.size(); ++i)
    { SafeTerm(m_pMesh[i]); }
    m_pMesh.clear();
    m_pMesh.shrink_to_fit();

    // マテリアル破棄.
    m_Material.Term();

    // ライト破棄.
    SafeDelete(m_pLight);

    // 変換バッファ破棄.
    for(size_t i=0; i<m_Transform.size(); ++i)
    { SafeTerm(m_Transform[i]); }
    m_Transform.clear();
    m_Transform.shrink_to_fit();
}

//-----------------------------------------------------------------------------
//      描画時の処理です.
//-----------------------------------------------------------------------------
void SampleApp::OnRender()
{
    

    // 更新処理.
    {
        float speed = 1.0f;
        float deltaTime = 1.0f / 60.0f;
        float deltaYaw = 0.0f;
        float deltaPitch = 0.0f;
        float sensitivity = 10.0f;
        float aspect = static_cast<float>(m_Width) / static_cast<float>(m_Height);

        m_front = m_targetPos - m_eyePos;
        m_front.Normalize();

        m_right = m_upward.Cross(m_front);
        m_right.Normalize();

        m_upward = m_front.Cross(m_right);
        m_upward.Normalize();

        Vector3 Angle_Movement = Vector3::Zero;
        m_Yaw -= sensitivity * (static_cast<float>(m_xMove) / m_Width) * DirectX::XM_PI / aspect;
        m_Pitch += sensitivity * (static_cast<float>(m_yMove) / m_Height) * DirectX::XM_PI;

        const float limit = DirectX::XM_PIDIV2 - 0.01f;
        m_Pitch = std::clamp(m_Pitch, -limit, limit);

        Vector3 eyeToTarget = m_eyePos - m_targetPos;
        m_Distance = eyeToTarget.Length();

        Quaternion qPitch = Quaternion::CreateFromAxisAngle(m_right,m_Pitch);
        Quaternion qYaw = Quaternion::CreateFromAxisAngle(m_upward, m_Yaw);
        Quaternion qRot = qPitch * qYaw;

        Vector3 direction = Vector3::Transform(m_front, qRot);
        direction.Normalize();
        m_eyePos = m_targetPos - (direction * m_Distance);
        
        m_xMove = 0;
        m_yMove = 0;
        m_Yaw = 0.0f;
        m_Pitch = 0.0f;

        // 移動処理
        Vector3 m_CameraMove = Vector3::Zero;
        if (m_MoveForward)  m_CameraMove = m_CameraMove + m_upward;
        if (m_MoveBackward) m_CameraMove = m_CameraMove - m_upward;
        if (m_MoveLeft)     m_CameraMove = m_CameraMove + m_right;
        if (m_MoveRight)    m_CameraMove = m_CameraMove - m_right;
        if (m_Wheel > 0)    m_CameraMove = m_CameraMove + m_front;
        if (m_Wheel < 0)    m_CameraMove = m_CameraMove - m_front;
        // カメラの移動量が0でなければ、正規化してスケーリングし、カメラ位置とターゲット位置を更新
        if (m_CameraMove != Vector3::Zero)
        {
            m_CameraMove.Normalize();
            if (m_Wheel > 0 || m_Wheel < 0) {
                m_CameraMove *= (m_zoomscale * speed * deltaTime);
            }
            else {
                m_CameraMove *= (speed * deltaTime);
            }
            m_eyePos = m_eyePos + m_CameraMove;
            if (m_Wheel == 0)m_targetPos += m_CameraMove;
        }
        m_Wheel = 0;

        //カメラの情報の更新
        auto pTransform = m_Transform[m_FrameIndex]->GetPtr<Transform>();
        pTransform->World = Matrix::CreateRotationY(m_RotateAngle);
        pTransform->View = Matrix::CreateLookAt(m_eyePos, m_targetPos, m_upward);

        pTransform->InvView = pTransform->View;
        pTransform->InvView.Invert();

        pTransform->InvView = pTransform->InvView.Transpose();
        pTransform->InvProj = pTransform->InvProj.Transpose();

        //ライトバッファの更新
        auto pLight = m_pLight->GetPtr<LightBuffer>();
        pLight->LightPosition = Vector4(0.0f, -100.0f, 1500.0f, 0.0);//Vector4(m_eyePos.x, m_eyePos.y, m_eyePos.z, 0.0f);
        pLight->LightColor = Color(1.0f, 1.0f, 1.0f, m_LightIntensity);
        pLight->CameraPosition = Vector4(m_eyePos.x, m_eyePos.y, m_eyePos.z, 0.0f);

        m_cameraBuffer = m_Transform[m_FrameIndex]->GetConstantBuffer();
    }
    //##########################################################
    //　　　GUIの処理の開始
    //##########################################################
    m_ImGuiUtil.ShowPanel(m_Width, m_Height, m_RenderType, this);

    // コマンドリストの記録を開始.
    auto pCmd = m_CommandList.Reset();

    // 書き込み用リソースバリア設定.
    DirectX::TransitionResource(pCmd,
        m_ColorTarget[m_FrameIndex].GetResource(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    // ディスクリプタ取得.
    auto handleRTV = m_ColorTarget[m_FrameIndex].GetHandleRTV();
    auto handleDSV = m_DepthTarget.GetHandleDSV();

    // レンダーターゲットを設定.
    pCmd->OMSetRenderTargets(1, &handleRTV->HandleCPU, FALSE, &handleDSV->HandleCPU);
    pCmd->RSSetViewports(1, &m_Viewport);
    pCmd->RSSetScissorRects(1, &m_Scissor);

    //現在のRENEDR_TYPEの確認
    std::array<float, 4> clearColor = std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f};
    switch (m_RenderType) {
    case RENDER_TYPE::RASTERIZE:
        clearColor = { 0.4f, 0.4f, 0.4f, 1.0f };
        break;

    case RENDER_TYPE::RAYTRACE:
        clearColor = { 0.6f, 0.6f, 0.6f, 1.0f };
        break;

    /*case RENDER_TYPE::NORMAL:
        clearColor = { 0.6f, 0.0f, 0.0f, 1.0f };
        break;

    case RENDER_TYPE::BASECOLOR:
        clearColor = { 0.0f, 0.6f, 0.0f, 1.0f };
        break;

    case RENDER_TYPE::ROUGHNESS:
        clearColor = { 0.0f, 0.0f, 0.6f, 1.0f };
        break;

    case RENDER_TYPE::METALLIC:
        clearColor = { 0.6f, 0.6f, 0.0f, 1.0f };
        break;*/

    default:
        clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        break;
    }

    switch(m_RenderType){

        case RENDER_TYPE::RASTERIZE:
            // レンダーターゲットをクリア.
            pCmd->ClearRenderTargetView(handleRTV->HandleCPU, clearColor.data(), 0, nullptr);
            // 深度ステンシルビューをクリア.
            pCmd->ClearDepthStencilView(handleDSV->HandleCPU, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

            // 描画処理.
            {
                ID3D12DescriptorHeap* const pHeaps[] = {
                    m_pPool[POOL_TYPE_RES]->GetHeap()
                };

                pCmd->SetGraphicsRootSignature(m_pRootSig.Get());
                pCmd->SetDescriptorHeaps(1, pHeaps);
                pCmd->SetGraphicsRootConstantBufferView(0, m_Transform[m_FrameIndex]->GetAddress());
                pCmd->SetGraphicsRootConstantBufferView(1, m_pLight->GetAddress());
                pCmd->SetPipelineState(m_pPSO.Get());

                for (size_t i = 0; i < m_pMesh.size(); ++i)
                {
                    // マテリアルIDを取得.
                    auto id = m_pMesh[i]->GetMaterialId();

                    // 定数バッファを設定.
                    pCmd->SetGraphicsRootConstantBufferView(2, m_Material.GetBufferAddress(i));

                    // テクスチャを設定.
                    pCmd->SetGraphicsRootDescriptorTable(3, m_Material.GetTextureHandle(id, TU_BASE_COLOR));
                    pCmd->SetGraphicsRootDescriptorTable(4, m_Material.GetTextureHandle(id, TU_NORMAL));
                    pCmd->SetGraphicsRootDescriptorTable(5, m_Material.GetTextureHandle(id, TU_ROUGHNESS));
                    pCmd->SetGraphicsRootDescriptorTable(6, m_Material.GetTextureHandle(id, TU_METALLIC));

                    // メッシュを描画.
                    m_pMesh[i]->Draw(pCmd);
                }

            }
            break;

        case RENDER_TYPE::RAYTRACE:
            CreateTopLevelAS(m_instances, true);

            std::vector<ID3D12DescriptorHeap*> heaps = { m_srvUavHeap.Get() };

            pCmd->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());

            //ヘルパ関数を用いているので、ResourceBarrierも実行している。
            DirectX::TransitionResource(pCmd,
                m_outputResource.Get(),
                D3D12_RESOURCE_STATE_COPY_SOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            D3D12_DISPATCH_RAYS_DESC desc = {};

            uint32_t rayGenerationSectionSizeInBytes =
                m_sbtHelper.GetRayGenSectionSize();
            desc.RayGenerationShaderRecord.StartAddress =
                m_sbtStorage->GetGPUVirtualAddress();
            desc.RayGenerationShaderRecord.SizeInBytes =
                rayGenerationSectionSizeInBytes;


            uint32_t missSectionSizeInBytes = m_sbtHelper.GetMissSectionSize();
            desc.MissShaderTable.StartAddress =
                m_sbtStorage->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes;
            desc.MissShaderTable.SizeInBytes = missSectionSizeInBytes;
            desc.MissShaderTable.StrideInBytes = m_sbtHelper.GetMissEntrySize();

            uint32_t hitGroupsSectionSize = m_sbtHelper.GetHitGroupSectionSize();
            desc.HitGroupTable.StartAddress = m_sbtStorage->GetGPUVirtualAddress() +
                rayGenerationSectionSizeInBytes +
                missSectionSizeInBytes;
            desc.HitGroupTable.SizeInBytes = hitGroupsSectionSize;
            desc.HitGroupTable.StrideInBytes = m_sbtHelper.GetHitGroupEntrySize();

            desc.Width = m_Width;
            desc.Height = m_Height;
            desc.Depth = 1;

            // Bind the raytracing pipeline
            pCmd->SetPipelineState1(m_rtStateObject.Get());
            // Dispatch the rays and write to the raytracing output
            pCmd->DispatchRays(&desc);


            DirectX::TransitionResource(
                pCmd, m_outputResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

            DirectX::TransitionResource(
                pCmd, m_ColorTarget[m_FrameIndex].GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);

            pCmd->CopyResource(m_ColorTarget[m_FrameIndex].GetResource(), m_outputResource.Get());

            DirectX::TransitionResource(
                pCmd, m_ColorTarget[m_FrameIndex].GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);

            break;
    }
    
    
    // ImGui 描画処理を追加.
    ID3D12DescriptorHeap* heaps[] = { m_ImGuiUtil.GetSRVHeap() };
    pCmd->SetDescriptorHeaps(1, heaps);
    m_ImGuiUtil.Render(pCmd);

    // 表示用リソースバリア設定.
    DirectX::TransitionResource(pCmd,
        m_ColorTarget[m_FrameIndex].GetResource(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);

    // コマンドリストの記録を終了.
    pCmd->Close();

    // コマンドリストを実行.
    ID3D12CommandList* pLists[] = { pCmd };
    m_pQueue->ExecuteCommandLists( 1, pLists );

    // 画面に表示.
    Present(1);
}
//-----------------------------------------------------------------------------
//      ウィンドウプロシージャです.
//-----------------------------------------------------------------------------

#define TEXTMAX 10000

// グローバル変数:
HINSTANCE hInst;                          // 現在のインターフェイス
// リッチエディットの使用するために用いる変数宣言
TCHAR strPath[MAX_PATH + 1];    // DLLのパス
HINSTANCE hRtLib;               // インスタンスハンドル
HWND hRichEdit;                 // ウィンドウハンドル

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


void SampleApp::OnMsgProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    
    // まずImGui側のメッセージ処理を呼ぶ
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wp, lp))
        return;  // ImGuiがこのメッセージを処理したなら終了//「もうアプリ側では追加の処理をする必要がない」という意思表示

    switch (msg) {
    case WM_KEYDOWN:
        switch (uint32_t(wp)) {
        case VK_ESCAPE: PostQuitMessage(0); break;
        case 'R':NextEnumName(m_RenderType); break;
        case 'W':
            std::cout << "W_Key input!" << std::endl; 
            m_MoveForward = true;
            break;
        case 'A':
            std::cout << "A_Key input!" << std::endl;
            m_MoveLeft = true;
            break;
        case 'S':
            std::cout << "S_Key input!" << std::endl; 
            m_MoveBackward = true;
            break;
        case 'D':
            std::cout << "D_Key input!" << std::endl; 
            m_MoveRight = true;
            break;
        case 'E':std::cout << static_cast<int>(m_RenderType) << std::endl; break;
        case VK_RIGHT:m_RotateAngle += 0.025f; break;
        case VK_LEFT:m_RotateAngle -= 0.025f; break;
        case VK_UP:break;
        case VK_DOWN:break;
        case VK_SPACE: std::cout << "(" << m_Height << ", " << m_Height << ")" << std::endl; break;
        }
        break;

    case WM_KEYUP:
        switch (uint32_t(wp)) {
            case 'W':
                std::cout << "W_Key Up!" << std::endl;
                m_MoveForward = false;
                break;
            case 'A':
                std::cout << "A_Key Up!" << std::endl;
                m_MoveLeft = false;
                break;
            case 'S':
                std::cout << "S_Key Up!" << std::endl;
                m_MoveBackward = false;
                break;
            case 'D':
                std::cout << "D_Key Up!" << std::endl;
                m_MoveRight = false;
                break;
        }
    
    case WM_LBUTTONDOWN:
        m_xPos = GET_X_LPARAM(lp);
        m_yPos = GET_Y_LPARAM(lp);
        break;


    case WM_LBUTTONUP:
        m_xPos = 0;
        m_yPos = 0;
        m_xMove = 0;
        m_yMove = 0;
        break;

    case WM_MOUSEMOVE:
        if (ImGui::GetIO().WantCaptureMouse) break;
        if (wp & MK_LBUTTON) {
            m_WindowEvent->UpdateState();
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);

            int dx = x - m_xPos;
            int dy = y - m_yPos;

            if (dx == 0 && dy == 0) {
                m_xMove = 0;
                m_yMove = 0;
            }
            else {
                m_xMove = dx;
                m_yMove = dy;
                std::cout << "pApp->m_xMove,pApp->m_yMove : " << m_xMove << " , " << m_yMove << std::endl;
                m_xPos = x;
                m_yPos = y;
            }
        }
        break;

    case WM_MOUSEWHEEL:
        m_Wheel = GET_WHEEL_DELTA_WPARAM(wp);
        std::cout << " pApp->m_Wheel = " << m_Wheel << std::endl;
        break;
    }
    
}

DWORD CALLBACK MyReadProc(DWORD_PTR dwCookie, LPBYTE pbBuf, LONG cb, LONG* pcb)//引数（pbBuf, cb, pcb）はリッチエディットが内部で管理
{
    ReadFile((HANDLE)dwCookie, pbBuf, cb, (LPDWORD)pcb, NULL);
    return(FALSE);
}


//
void SampleApp::CheckRaytracingSupport() {
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    ThrowIfFailed(m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));
    if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
        throw std::runtime_error("Raytracing not supported on device");

    switch (options5.RaytracingTier)
    {
    case D3D12_RAYTRACING_TIER_NOT_SUPPORTED:
        std::cout << "Raytracing: NOT SUPPORTED" << std::endl;
        break;

    case D3D12_RAYTRACING_TIER_1_0:
        std::cout << "Raytracing: Tier 1.0 (DXR 1.0 / RS5)" << std::endl;
        break;

    case D3D12_RAYTRACING_TIER_1_1:
        std::cout << "Raytracing: Tier 1.1 (DXR 1.1)" << std::endl;
        break;

    default:
        std::cout << "Raytracing: Unknown tier" << std::endl;
        break;
    }

}

SampleApp::AccelerationStructureBuffers
SampleApp::CreateBottomLevelAS(std::vector<std::pair<ComPtr<ID3D12Resource>, uint32_t>> vVertexBuffers,
    std::vector<std::pair<ComPtr<ID3D12Resource>, uint32_t>> vIndexBuffers) {
    nv_helpers_dx12::BottomLevelASGenerator bottomLevelAS;

    for (size_t i = 0; i < vVertexBuffers.size(); i++) {
        if (i < vIndexBuffers.size() && vIndexBuffers[i].second > 0)
            bottomLevelAS.AddVertexBuffer(vVertexBuffers[i].first.Get(), 0,
                vVertexBuffers[i].second, sizeof(Vertex),
                vIndexBuffers[i].first.Get(), 0,
                vIndexBuffers[i].second, nullptr, 0, true);

        else
            bottomLevelAS.AddVertexBuffer(vVertexBuffers[i].first.Get(), 0,
                vVertexBuffers[i].second, sizeof(Vertex), 0, 0);
    }

    UINT64 scratchSizeInBytes = 0;
    UINT64 resultSizeInBytes = 0;

    bottomLevelAS.ComputeASBufferSizes(m_pDevice.Get(), false, &scratchSizeInBytes, &resultSizeInBytes);

    AccelerationStructureBuffers buffers;
    buffers.pScratch = nv_helpers_dx12::CreateBuffer(
        m_pDevice.Get(), scratchSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON,
        nv_helpers_dx12::kDefaultHeapProps);

    buffers.pResult = nv_helpers_dx12::CreateBuffer(
        m_pDevice.Get(), resultSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        nv_helpers_dx12::kDefaultHeapProps
    );

    bottomLevelAS.Generate(/*m_commandList.Get()*/m_CommandList.GetCommandList(), buffers.pScratch.Get(),
        buffers.pResult.Get(), false, nullptr);

    return buffers;
}

void SampleApp::CreateTopLevelAS(
    const std::vector<std::pair<ComPtr<ID3D12Resource>, DirectX::XMMATRIX>>& instances,// pair of bottom level AS and matrix of the instance
    bool updateOnly)// If true the top-level AS will only be refitted and not rebuilt from scratch
{
    // #DXR Extra - Refitting
    // gather all the instances into the builder helper
    if (!updateOnly) {
        std::cout << instances.size() << std::endl;
        for (size_t i = 0; i < instances.size(); i++) {
            m_topLevelASGenerator.AddInstance(
                instances[i].first.Get(), instances[i].second, static_cast<UINT>(i),
                static_cast<UINT>(2 * i));
        }

        /*Bottom - Level AS の構築には、実際の AS（結果）に加えて、一時データを保存するためのスクラッチ領域が必要になります。
        また Top - Level AS の場合は、インスタンスディスクリプタも GPU メモリ上に配置する必要があります。
        この呼び出しは、それぞれに必要なメモリ量（スクラッチ、結果、インスタンスディスクリプタ）を出力し、
        アプリケーションが対応するメモリを確保できるようにします。*/
        UINT64 scratchSize, resultSize, instanceDescsSize;

        m_topLevelASGenerator.ComputeASBufferSizes(
            m_pDevice.Get(), true, &scratchSize, &resultSize, &instanceDescsSize);

        m_topLevelASBuffers.pScratch = nv_helpers_dx12::CreateBuffer(
            m_pDevice.Get(), scratchSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nv_helpers_dx12::kDefaultHeapProps);

        m_topLevelASBuffers.pResult = nv_helpers_dx12::CreateBuffer(
            m_pDevice.Get(), resultSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            nv_helpers_dx12::kDefaultHeapProps);

        m_topLevelASBuffers.pInstanceDesc = nv_helpers_dx12::CreateBuffer(
            m_pDevice.Get(), instanceDescsSize, D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

        if (!m_topLevelASBuffers.pInstanceDesc) {
            // エラー出力
            std::cout << "pInstanceDesc is nullptr!" << std::endl;
        }
    }

    m_topLevelASGenerator.Generate(m_CommandList.GetCommandList(),
        m_topLevelASBuffers.pScratch.Get(),
        m_topLevelASBuffers.pResult.Get(),
        m_topLevelASBuffers.pInstanceDesc.Get(),
        updateOnly, m_topLevelASBuffers.pResult.Get());

}

//-----------------------------------------------------------------------------
//
// Combine the BLAS and TLAS builds to construct the entire acceleration
// structure required to raytrace the scene
//

//###############################################
//　いくつか修正が必要だ
//###############################################

void SampleApp::CreateAccelerationStructures() {
    //自分が描画するメッシュだけ記述する。
    //AccelerationStructureBuffers bottomLevelBuffers = CreateBottomLevelAS(
    //    { {m_vertexBuffer.Get(), 4} }, { {m_indexBuffer.Get(),12} });//std::vector<std::pair~>>なので

    AccelerationStructureBuffers planeBottomLevelBuffers =
        CreateBottomLevelAS({ {m_planeBuffer.Get(), 6} });

    m_instances = {
        {planeBottomLevelBuffers.pResult, DirectX::XMMatrixTranslation(0, 0, 2.0f)}
    };

    CreateTopLevelAS(m_instances,false);

    m_CommandList.GetCommandList()->Close();
    ID3D12CommandList* ppCommandLists[] = { m_CommandList.GetCommandList() };
    m_pQueue->ExecuteCommandLists(1, ppCommandLists);
    m_Fence.AddFenceCounter();
    m_pQueue->Signal(m_Fence.GetFence(), m_Fence.GetFenceCounter());

    m_Fence.GetFence()->SetEventOnCompletion(m_Fence.GetFenceCounter(), m_Fence.GetFenceEvent());
    WaitForSingleObject(m_Fence.GetFenceEvent(), INFINITE);



    //ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));
    m_CommandList.Reset();

    /*m_bottomLevelAS = bottomLevelBuffers.pResult;*/
}

ComPtr<ID3D12RootSignature> SampleApp::CreateRayGenSignature() {
    nv_helpers_dx12::RootSignatureGenerator rsc;
    rsc.AddHeapRangesParameter(
        {
         {0,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_UAV,0},
         {0,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1},
         {0,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_CBV,2}
        }
    );

    return rsc.Generate(m_pDevice.Get(), true);
}

ComPtr<ID3D12RootSignature> SampleApp::CreateHitSignature() {
    nv_helpers_dx12::RootSignatureGenerator rsc;
    rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0/*t0*/);
    rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 1/*t1*/);//indices
    rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 0);
    rsc.AddHeapRangesParameter({ {
        2/*t2*/,1,0,D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1/*2nd slot of the heap*/
    } });
    /*各インスタンスごとに違うデータ（ここでは頂点カラー）をレイトレーシングのシェーダに渡したいので、
    共有ヒープ上の1つのバッファを指すのではなく、SBTから「root parameter（GPU仮想アドレス）」として直接渡す方式を使う。*/
    return rsc.Generate(m_pDevice.Get(), true);
}

ComPtr<ID3D12RootSignature> SampleApp::CreateMissSignature() {
    nv_helpers_dx12::RootSignatureGenerator rsc;
    return rsc.Generate(m_pDevice.Get(), true);
}

void SampleApp::CreateRaytracingPipeline() {
    nv_helpers_dx12::RayTracingPipelineGenerator pipeline(m_pDevice.Get());

    //IDxcBlob*型の返り値 //"C:\Users\7544k\Github\DirectX12\rasterization\DirectX12Renderer_GGX\Sample\res\RayGen.hlsl"
    
    //m_rayGenLibrary = nv_helpers_dx12::CompileShaderLibrary(/*L"RayGen.hlsl"*/L"../../../Sample/res/RayGen.hlsl");
    m_rayGenLibrary = nv_helpers_dx12::CompileShaderLibrary(L"C:/Users/7544k/Github/DirectX12/rasterization/Direct3D12Renderer/Sample/res/RayGen.hlsl");
    //m_missLibrary = nv_helpers_dx12::CompileShaderLibrary(/*L"Miss.hlsl"*/L"../../../Sample/res/Miss.hlsl");
    m_missLibrary = nv_helpers_dx12::CompileShaderLibrary(L"C:/Users/7544k/Github/DirectX12/rasterization/Direct3D12Renderer/Sample/res/Miss.hlsl");
    //m_hitLibrary = nv_helpers_dx12::CompileShaderLibrary(/*L"Hit.hlsl"*/L"../../../Sample/res/Hit.hlsl");
    m_hitLibrary = nv_helpers_dx12::CompileShaderLibrary(L"C:/Users/7544k/Github/DirectX12/rasterization/Direct3D12Renderer/Sample/res/Hit.hlsl");
    //m_shadowLibrary = nv_helpers_dx12::CompileShaderLibrary(/*L"ShadowRay.hlsl"*/L"../../../Sample/res/ShadowRay.hlsl");
    m_shadowLibrary = nv_helpers_dx12::CompileShaderLibrary(L"C:/Users/7544k/Github/DirectX12/rasterization/Direct3D12Renderer/Sample/res/ShadowRay.hlsl");


    //m_librariesにLibraryをemplace_backしている。
    pipeline.AddLibrary(m_rayGenLibrary.Get(), { L"RayGen" });
    pipeline.AddLibrary(m_missLibrary.Get(), { L"Miss" });
    pipeline.AddLibrary(m_hitLibrary.Get(), { L"ClosestHit", L"PlaneClosestHit" });
    pipeline.AddLibrary(m_shadowLibrary.Get(), { L"ShadowClosestHit", L"ShadowMiss" });

    m_rayGenSignature = CreateRayGenSignature();
    m_missSignature = CreateMissSignature();
    m_hitSignature = CreateHitSignature();
    m_shadowSignature = CreateHitSignature();

    pipeline.AddHitGroup(L"HitGroup", L"ClosestHit");
    pipeline.AddHitGroup(L"PlaneHitGroup", L"PlaneClosestHit");
    pipeline.AddHitGroup(L"ShadowHitGroup", L"ShadowClosestHit");

    //エクスポート名（関数名：右項）内で、ルートシグニチャ（左項）を利用できるようにする。
    pipeline.AddRootSignatureAssociation(m_rayGenSignature.Get(), { L"RayGen" });
    //pipeline.AddRootSignatureAssociation(m_missSignature.Get(), { L"Miss" }); //後の行で上書きされるため、コメントアウト
    //pipeline.AddRootSignatureAssociation(m_hitSignature.Get(), { L"HitGroup" }); //後の行で上書きされるため、コメントアウト

    pipeline.AddRootSignatureAssociation(m_shadowSignature.Get(), { L"ShadowHitGroup" });
    pipeline.AddRootSignatureAssociation(m_missSignature.Get(), { L"Miss", L"ShadowMiss" });
    pipeline.AddRootSignatureAssociation(m_hitSignature.Get(), { L"HitGroup", L"PlaneHitGroup" });

    pipeline.SetMaxPayloadSize(4 * sizeof(float)); // RGB + distance
    pipeline.SetMaxAttributeSize(2 * sizeof(float)); // barycentric coordinates
    pipeline.SetMaxRecursionDepth(2);


    // Compile the pipeline for execution on the GPU
    m_rtStateObject = pipeline.Generate();


    ThrowIfFailed(
        m_rtStateObject->QueryInterface(IID_PPV_ARGS(&m_rtStateObjectProps)));

}

void SampleApp::CreateRaytracingOutputBuffer() {
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.DepthOrArraySize = 1;
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    //resDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    resDesc.Width = m_Width;
    resDesc.Height = m_Height;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;

    ThrowIfFailed(m_pDevice->CreateCommittedResource(
        &nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
        D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr,
        IID_PPV_ARGS(&m_outputResource)));
}

//「複数リソース用のディスクリプタヒープ」を作成し、各スロットにビューを書き込むことで、
// シェーダーからまとめて利用できるようにしています。
void SampleApp::CreateShaderResourceHeap() {

    //3つ分のスロット（場所）を持つヒープが作られます。描画結果、AS構造、
    /*m_srvUavHeap = nv_helpers_dx12::CreateDescriptorHeap(
        m_pDevice.Get(), 3, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);*/
        // ヘルパーを使わず直接作成する
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 3;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // 明示的にフラグを立てる
    heapDesc.NodeMask = 0;

    ThrowIfFailed(m_pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvUavHeap)));

    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_srvUavHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    m_pDevice->CreateUnorderedAccessView(m_outputResource.Get(), nullptr, &uavDesc, srvHandle);
    srvHandle.ptr += m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location =
        m_topLevelASBuffers.pResult->GetGPUVirtualAddress();
    // Write the acceleration structure view in the heap
    m_pDevice->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);

    // #DXR Extra: Perspective Camera
    // Add the constant buffer for the camera after the TLAS
    srvHandle.ptr += m_pDevice->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Describe and create a constant buffer view for the camera
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = m_cameraBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = m_cameraBufferSize;
    /*cbvDesc.BufferLocation = m_Transform[m_FrameIndex]->GetAddress();
    cbvDesc.SizeInBytes = m_Transform[m_FrameIndex]->GetSizeInByte();*/
    m_pDevice->CreateConstantBufferView(&cbvDesc, srvHandle);

}

void SampleApp::CreateShaderBindingTable() {
    if (m_srvUavHeap == nullptr) {
        OutputDebugStringA("！！！エラー：ヒープがまだ作成されていません！！！\n");
        std::cout << "！！！エラー：ヒープがまだ作成されていません！！！" << std::endl;
        return;
    }
    else {
        std::cout << "ヒープは作成されている！！！" << std::endl;
    }
    m_sbtHelper.Reset();

    D3D12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle = 
        m_srvUavHeap->GetGPUDescriptorHandleForHeapStart();

    auto heapPointer = reinterpret_cast<UINT64*>(srvUavHeapHandle.ptr);
    // The ray generation only uses heap data
    m_sbtHelper.AddRayGenerationProgram(L"RayGen", { heapPointer });
    // The miss and hit shaders do not access any external resources: instead they
    // communicate their results through the ray payload
    m_sbtHelper.AddMissProgram(L"Miss", {});
    // #DXR Extra - Another ray type
    m_sbtHelper.AddMissProgram(L"ShadowMiss", {});


    //###############################################
    //　いくつか修正が必要だ
    //###############################################
    //{
    //    m_sbtHelper.AddHitGroup(
    //        L"HitGroup",
    //        { (void*)(m_mengerVB->GetGPUVirtualAddress()),
    //         (void*)(m_mengerIB->GetGPUVirtualAddress()),
    //         (void*)(m_perInstanceConstantBuffers[0]->GetGPUVirtualAddress()) });
    //    // #DXR Extra - Another ray type
    //    m_sbtHelper.AddHitGroup(L"ShadowHitGroup", {});
    //}
    // 
    // The plane also uses a constant buffer for its vertex colors
    // #DXR Extra: Per-Instance Data
    // Adding the plane

    m_sbtHelper.AddHitGroup(L"PlaneHitGroup", { heapPointer });
    // #DXR Extra - Another ray type
    m_sbtHelper.AddHitGroup(L"ShadowHitGroup", {});

    uint32_t sbtSize = m_sbtHelper.ComputeSBTSize();
    m_sbtStorage = nv_helpers_dx12::CreateBuffer(
        m_pDevice.Get(), sbtSize, D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

    if (!m_sbtStorage) {
        throw std::logic_error("Could not allocate the shader biding table.");
    }

    //「シェーダーバインディングテーブル（SBT）」の内容をGPU用バッファ（sbtBuffer：m_sbtStorage.Get()）に書き込む処理
    //マッピングを用いて、書き込みを行う。
    //　CopyShaderData を使って、
    //-レイ生成シェーダー（m_rayGen）
    //- ミスシェーダー（m_miss）
    //- ヒットグループ（m_hitGroup）
    //の順に、
    //「シェーダーID（識別子）」＋「リソースポインタやルート定数」をSBTバッファに順番に書き込みます。
    //SBTのエントリ：「どのシェーダーを使うか」と「そのシェーダーに渡すリソースや定数は何か」をGPUに伝えるための情報セット

    m_sbtHelper.Generate(m_sbtStorage.Get(), m_rtStateObjectProps.Get());
}

void SampleApp::CreateCameraBuffer() {
    //uint32_t nbMatrix = 4;// view, perspective, viewInv, perspectiveInv
    uint32_t nbMatrix = 5;// World, view, perspective, viewInv, perspectiveInv
    m_cameraBufferSize = nbMatrix * sizeof(DirectX::XMMATRIX);
    std::cout << "m_cameraBufferSize : " << m_cameraBufferSize << std::endl;
    // 2. 256アライメントに切り上げ (320 -> 512)
    m_cameraBufferSize = (m_cameraBufferSize + 255) & ~255;

    std::cout << "m_cameraBufferSize (Aligned): " << m_cameraBufferSize << std::endl;
    //create the constant buffer for all matrices
    m_cameraBuffer = nv_helpers_dx12::CreateBuffer(
        m_pDevice.Get(), m_cameraBufferSize, D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

    // #DXR Extra - Refitting
    // Create a descriptor heap that will be used by the rasterization shaders:
    // Camera matrices and per-instance matrices
    m_constHeap = nv_helpers_dx12::CreateDescriptorHeap(
        m_pDevice.Get(), 2, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

    // Describe and create the constant buffer view.
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = m_cameraBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = m_cameraBufferSize;

    // Get a handle to the heap memory on the CPU side, to be able to write the
    // descriptors directly
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle =
        m_constHeap->GetCPUDescriptorHandleForHeapStart();
    m_pDevice->CreateConstantBufferView(&cbvDesc, srvHandle);

    // #DXR Extra - Refitting
    // Add the per-instance buffer
    srvHandle.ptr += m_pDevice->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = static_cast<UINT>(m_instances.size());
    srvDesc.Buffer.StructureByteStride = sizeof(InstanceProperties);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    // Write the per-instance buffer view in the heap
    m_pDevice->CreateShaderResourceView(m_instanceProperties.Get(), &srvDesc,
        srvHandle);
}

void SampleApp::UpdateCameraBuffer() {

    std::vector<DirectX::XMMATRIX> matrices(4);
    // view, perspective, viewInv, perspectiveInv
    //XMMATRIX は内部的に r[4] という XMVECTOR（4要素ベクトル）が4つ並んだ構造体
    //matrices[0].r は XMVECTOR[4] の配列。
    //matrices[0].r->m128_f32[0] は、最初のXMVECTOR（r[0]）の最初のfloat成分（m128_f32[0]）を指します。
    auto pTransform = m_Transform[m_FrameIndex]->GetPtr<Transform>();
    DirectX::XMFLOAT4X4 mat4x4;
    DirectX::XMStoreFloat4x4(&mat4x4, pTransform->View);
    const float* ptr = &mat4x4.m[0][0];
    memcpy(&matrices[0].r->m128_f32[0], ptr, 16 * sizeof(float));
    //const glm::mat4& mat = nv_helpers_dx12::CameraManip.GetMatrix();
    //memcpy(&matrices[0].r->m128_f32[0], glm::value_ptr(mat), 16 * sizeof(float));

    float fovAngleY = 45.0f * DirectX::XM_PI / 180.0f;
    matrices[1] = DirectX::XMMatrixPerspectiveFovRH(fovAngleY, m_aspectRatio, 0.1f, 1000.0f);

    // Raytracing has to do the contrary of rasterization: rays are defined in
    // camera space, and are transformed into world space. To do this, we need to
    // store the inverse matrices as well.
    DirectX::XMVECTOR det;
    matrices[2] = XMMatrixInverse(&det, matrices[0]);
    matrices[3] = XMMatrixInverse(&det, matrices[1]);

    // Copy the matrix contents
    uint8_t* pData;
    ThrowIfFailed(m_cameraBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, matrices.data(), m_cameraBufferSize);
    m_cameraBuffer->Unmap(0, nullptr);
}

//--------------------------------------------------------------------------------------------------
// Allocate memory to hold per-instance information
// #DXR Extra - Refitting
/*
この関数 D3D12HelloTriangle::CreateInstancePropertiesBuffer() は、
「各インスタンスごとの情報（行列など）を格納するバッファ（GPUリソース）」を確保
*/
void SampleApp::CreateInstancePropertiesBuffer() {
    uint32_t bufferSize = ROUND_UP(
        static_cast<uint32_t>(m_instances.size()) * sizeof(InstanceProperties),
        D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    // Create the constant buffer for all matrices
    m_instanceProperties = nv_helpers_dx12::CreateBuffer(
        m_pDevice.Get(), bufferSize, D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);
}


//--------------------------------------------------------------------------------------------------
// Copy the per-instance data into the buffer
// #DXR Extra - Refitting
/*
この関数 D3D12HelloTriangle::UpdateInstancePropertiesBuffer() は、
「各インスタンスの最新の変換行列（objectToWorld）」をGPUバッファ（m_instanceProperties）に書き込む処理です。

m_instances には「各インスタンスのBLASリソースとワールド行列（XMMATRIX）」が格納されています。
m_instanceProperties は CreateInstancePropertiesBuffer() で確保した、インスタンスごとのデータ（InstanceProperties構造体）を格納するGPUバッファです。
この関数は、m_instanceProperties をCPUから書き込み可能な状態でMapし、各インスタンスのobjectToWorld行列を順番に格納します。
ループで current->objectToWorld = inst.second; として、各インスタンスのワールド行列をバッファにコピーしています。
最後にUnmapして、GPU側で使えるようにします。

*/
void SampleApp::UpdateInstancePropertiesBuffer() {
    InstanceProperties* current = nullptr;
    CD3DX12_RANGE readRange(
        0, 0); // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_instanceProperties->Map(0, &readRange,
        reinterpret_cast<void**>(&current)));
    for (const auto& inst : m_instances) {
        current->objectToWorld = inst.second;
        current++;
    }
    m_instanceProperties->Unmap(0, nullptr);
}

void SampleApp::CreatePlaneVB() {
    // Define the geometry for a plane.
    Vertex planeVertices[] = {
        {{-1.5f, -.8f, 01.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 0
        {{-1.5f, -.8f, -1.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 1
        {{01.5f, -.8f, 01.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 2
        {{01.5f, -.8f, 01.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 2
        {{-1.5f, -.8f, -1.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 1
        {{01.5f, -.8f, -1.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}  // 4
    };

    const UINT planeBufferSize = sizeof(planeVertices);
    std::cout << "planeBufferSize = " << planeBufferSize << std::endl;

    // Note: using upload heaps to transfer static data like vert buffers is not
    // recommended. Every time the GPU needs it, the upload heap will be
    // marshalled over. Please read up on Default Heap usage. An upload heap is
    // used here for code simplicity and because there are very few verts to
    // actually transfer.
    CD3DX12_HEAP_PROPERTIES heapProperty =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferResource =
        CD3DX12_RESOURCE_DESC::Buffer(planeBufferSize);
    ThrowIfFailed(m_pDevice->CreateCommittedResource(
        &heapProperty, D3D12_HEAP_FLAG_NONE, &bufferResource, //
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_planeBuffer)));

    // Copy the triangle data to the vertex buffer.
    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange(
        0, 0); // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_planeBuffer->Map(
        0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
    memcpy(pVertexDataBegin, planeVertices, sizeof(planeVertices));
    m_planeBuffer->Unmap(0, nullptr);

    if (!m_planeBuffer) {
        std::cout << "m_planeBuffer = nullptr" << std::endl;
    }
    else {
        std::cout << "m_planeBuffer != nullptr" << std::endl;
    }

    // Initialize the vertex buffer view.
    m_planeBufferView.BufferLocation = m_planeBuffer->GetGPUVirtualAddress();
    m_planeBufferView.StrideInBytes = sizeof(Vertex);
    m_planeBufferView.SizeInBytes = planeBufferSize;
}

void SampleApp::CreateGlobalConstantBuffer() {
    // Due to HLSL packing rules, we create the CB with 9 float4 (each needs to
    // start on a 16-byte boundary)
    DirectX::XMVECTOR bufferData[] = {
        // A
        DirectX::XMVECTOR{1.0f, 0.0f, 0.0f, 1.0f},
        DirectX::XMVECTOR{0.7f, 0.4f, 0.0f, 1.0f},
        DirectX::XMVECTOR{0.4f, 0.7f, 0.0f, 1.0f},

        // B
        DirectX::XMVECTOR{0.0f, 1.0f, 0.0f, 1.0f},
        DirectX::XMVECTOR{0.0f, 0.7f, 0.4f, 1.0f},
        DirectX::XMVECTOR{0.0f, 0.4f, 0.7f, 1.0f},

        // C
        DirectX::XMVECTOR{0.0f, 0.0f, 1.0f, 1.0f},
        DirectX::XMVECTOR{0.4f, 0.0f, 0.7f, 1.0f},
        DirectX::XMVECTOR{0.7f, 0.0f, 0.4f, 1.0f},
    };

    // Create our buffer
    m_globalConstantBuffer = nv_helpers_dx12::CreateBuffer(
        m_pDevice.Get(), sizeof(bufferData), D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

    // Copy CPU memory to GPU
    uint8_t* pData;
    ThrowIfFailed(m_globalConstantBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, bufferData, sizeof(bufferData));
    m_globalConstantBuffer->Unmap(0, nullptr);
}

void SampleApp::CreatePerInstanceConstantBuffers() {
    // Due to HLSL packing rules, we create the CB with 9 float4 (each needs to
    // start on a 16-byte boundary)
    DirectX::XMVECTOR bufferData[] = {
        // A
        DirectX::XMVECTOR{1.0f, 0.0f, 0.0f, 1.0f},
        DirectX::XMVECTOR{1.0f, 0.4f, 0.0f, 1.0f},
        DirectX::XMVECTOR{1.f, 0.7f, 0.0f, 1.0f},

        // B
        DirectX::XMVECTOR{0.0f, 1.0f, 0.0f, 1.0f},
        DirectX::XMVECTOR{0.0f, 1.0f, 0.4f, 1.0f},
        DirectX::XMVECTOR{0.0f, 1.0f, 0.7f, 1.0f},

        // C
        DirectX::XMVECTOR{0.0f, 0.0f, 1.0f, 1.0f},
        DirectX::XMVECTOR{0.4f, 0.0f, 1.0f, 1.0f},
        DirectX::XMVECTOR{0.7f, 0.0f, 1.0f, 1.0f},
    };

    m_perInstanceConstantBuffers.resize(3);
    int i(0);
    for (auto& cb : m_perInstanceConstantBuffers) {
        const uint32_t bufferSize = sizeof(DirectX::XMVECTOR) * 3;
        cb = nv_helpers_dx12::CreateBuffer(
            m_pDevice.Get(), bufferSize, D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);
        uint8_t* pData;
        ThrowIfFailed(cb->Map(0, nullptr, (void**)&pData));
        memcpy(pData, &bufferData[i * 3], bufferSize);
        cb->Unmap(0, nullptr);
        ++i;
    }
}