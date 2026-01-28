#include "ImGuiUtil.h"
#include <Winuser.h>
#include <combaseapi.h>
#include "SampleApp.h"
//#include "ImguiUtil.h"
#include <iostream>
#include <RenderType.h>
#include <commdlg.h>
#include <cstring>
#include <EnumUtil.h>
ImGuiUtil::ImGuiUtil() {}
ImGuiUtil::~ImGuiUtil() {}

void ImGuiUtil::Initialize(HWND hWnd, ID3D12Device* pDevice) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsLight();

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    pDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap));

    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX12_Init(pDevice, App::FrameCount,
        /*DXGI_FORMAT_R8G8B8A8_UNORM*/DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, m_srvHeap.Get(),
        m_srvHeap->GetCPUDescriptorHandleForHeapStart(),
        m_srvHeap->GetGPUDescriptorHandleForHeapStart());

    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
}

void ImGuiUtil::Finalize() {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiUtil::NewFrame() {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiUtil::AddTableTextRow(const char* label, const char* value) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s",label);
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%s",value);
}

void ImGuiUtil::Render(ID3D12GraphicsCommandList* pCommandList) {
    ImGui::Render();
    ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvHeap.Get() };
    pCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCommandList);
}

bool OpenFileDialog(char* outPath, size_t size)
{
    OPENFILENAMEA ofn = {};
    char file[260] = "";

    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "All Files\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = sizeof(file);
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn))
    {
        strncpy_s(outPath, size, file, _TRUNCATE);
        return true;
    }
    return false;
}

void ImGuiUtil::ShowPanel(uint32_t W, uint32_t H, RENDER_TYPE& type, SampleApp* app){
    static bool show_sub_window = false;
    //##########################################################
    //　　　GUIのパラメータ
    //##########################################################
    this->NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);  // スクリーン座標で指定
    ImGui::SetNextWindowSize(ImVec2(W * 0.3f, H * 0.45f), ImGuiCond_FirstUseEver);
    
    ImVec4 bgColor = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); // 任意のRGBA色
    //##########################################################
    //　　　メインウィンドウの生成
    //##########################################################
    ImGui::PushStyleColor(ImGuiCol_WindowBg, bgColor);
    ImGui::Begin("Debug Window", nullptr, ImGuiWindowFlags_None | ImGuiWindowFlags_AlwaysVerticalScrollbar);
    //##########################################################
    //　　　操作方法の設定
    //##########################################################
    ImGui::Text("RenderType");
    int current = static_cast<int>(type);
    if (ImGui::Combo("##RenderType", &current, RENDER_TYPE_NAMES, IM_ARRAYSIZE(RENDER_TYPE_NAMES))) {
        // 選択が変わったときの処理
        type = static_cast<RENDER_TYPE>(current);
    }

    if (ImGui::TreeNode("Setting")) {
        if (ImGui::BeginTable("setting", 2, ImGuiTableFlags_None)) {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f); // 1列目: ラベル
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);      // 2列目: スライダー（可変幅）

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Zoom_Intensity:");
            ImGui::TableSetColumnIndex(1);
            ImGui::SliderFloat("##m_zoomscale", &(app->m_zoomscale), 0.0f, 10.0f, "%.2f");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Light_Intensity:");
            ImGui::TableSetColumnIndex(1);
            ImGui::SliderFloat("##m_LightIntensity", &(app->m_LightIntensity), 0.0f, 1.0f, "%.2f");

            ImGui::EndTable();
        }
        ImGui::TreePop();
    }

    //static char importpath_mesh[256] = "";
    //ImGui::Text("Import Mesh");
    //ImGui::InputText("##File Path_mesh", importpath_mesh, sizeof(importpath_mesh));
    //ImGui::SameLine();
    //if (ImGui::Button("Import##Mesh")) {
    //    OpenFileDialog(importpath_mesh, sizeof(importpath_mesh));
    //    std::cout << "Import : " << importpath_mesh << std::endl;
    //}
    //ImGui::SameLine();
    //if (ImGui::Button("Apply##Mesh")) {
    //    //読み込み処理を記述
    //}

    //static char importpath_env[256] = "";
    //ImGui::Text("Import Environment Map");
    //ImGui::InputText("##File Path_env", importpath_env, sizeof(importpath_env));
    //ImGui::SameLine();
    //if (ImGui::Button("Import##Env")) {
    //    OpenFileDialog(importpath_env, sizeof(importpath_env));
    //    std::cout << "Import : " << importpath_env << std::endl;
    //}
    //ImGui::SameLine();
    //if (ImGui::Button("Apply##Env")) {
    //    //読み込み処理を記述
    //}

    if (ImGui::TreeNode("Help")) {
        if (ImGui::TreeNode("Keyboard Controls")) {
            if (ImGui::BeginTable("Keyboard & Mouse Controls", 2, /*ImGuiTableFlags_None*/ImGuiTableFlags_BordersInnerH)) {
                ImGui::TableSetupColumn("Input", ImGuiTableColumnFlags_WidthFixed, 30.0f);
                ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);

                AddTableTextRow("W/S", "Camera: Move Up/Down (Screen Space)");
                AddTableTextRow("A/D", "Camera: Move Left/Right (Screen Space)");

                ImGui::TableNextRow();
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Mouse Controls")) {
            if (ImGui::BeginTable("MouseTable", 2, ImGuiTableFlags_BordersInnerH)) {
                ImGui::TableSetupColumn("Button", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);

                AddTableTextRow("Left Drag", "Camera: Rotate (Quaternion)");
                AddTableTextRow("Wheel", "Camera: Zoom In / Out");

                ImGui::EndTable();
            }
            ImGui::TreePop();
        }

        ImGui::TreePop();
    }


    ImGui::End();
    ImGui::PopStyleColor(); // 必ず対応するPopが必要
}

