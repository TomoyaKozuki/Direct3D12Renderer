#pragma once
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <RenderType.h>
#include <App.h>

class ImGuiUtil {
public:
    ImGuiUtil();
    virtual ~ImGuiUtil();

    ID3D12DescriptorHeap* GetSRVHeap() const { return m_srvHeap.Get(); }

    void Initialize(HWND hWnd, ID3D12Device* pDevice);
    void Finalize();
    void NewFrame();
    void Render(ID3D12GraphicsCommandList* pCommandList);
    void ShowPanel(uint32_t W, uint32_t H, RENDER_TYPE& type,App* app);
    

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    void AddTableTextRow(const char* label, const char* value);
};

bool OpenFileDialog(char* outPath, size_t size);
/*
virtualをつける理由：ポリモーフィズム（継承クラスをポインタで扱う）を使ったときに正しくデストラクタが呼ばれないというバグの原因
virtualをつける → 子クラスで上書きできる関数（仮想関数）
仮想デストラクタにしないと子の破棄が中途半端になる
*/