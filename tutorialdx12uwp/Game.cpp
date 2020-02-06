//
// Game.cpp
//

#include "pch.h"
#include "Game.h"

extern void ExitGame();

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Game::Game() noexcept :
    m_window(nullptr),
    m_outputWidth(800),
    m_outputHeight(600),
    m_outputRotation(DXGI_MODE_ROTATION_IDENTITY),
    m_featureLevel(D3D_FEATURE_LEVEL_11_0),
    m_backBufferIndex(0),
    m_rtvDescriptorSize(0),
    m_fenceValues{}
{
}

void Game::Initialize(::IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation)
{
	// Definimos cuantas instancias generamos.
	numInstances = 10;

	// Definimos los límites del mundo.
	worldBoundaries = XMFLOAT3(10, 10, 10);

    // Inicializamos matrices de transformación
    XMStoreFloat4x4(&m_view, XMMatrixIdentity());
    XMStoreFloat4x4(&m_projection, XMMatrixIdentity());

	// Inicializamos las instancias
	ObjectData o;
	objects.clear();

	for (int i = 0; i < numInstances; i++) {
		XMStoreFloat4x4(&o.m_world, XMMatrixTranslation(0, 0, 0));

		// Ponerle posición aleatoria.
		float randomX = RandomNumber(-worldBoundaries.x, worldBoundaries.x);
		float randomY = RandomNumber(-worldBoundaries.y, worldBoundaries.y);
		float randomZ = RandomNumber(-worldBoundaries.z, worldBoundaries.z);
		o.m_pos = XMFLOAT3(randomX, randomY, randomZ);

		// Generar velocidad aleatoria.
		XMVECTOR randomV = randomNormalizedVector();
		XMStoreFloat3(&o.m_velocity, randomV);
		
		// Velocidad de movimiento.
		o.m_speed = 0.5f;

		// Poner en el vector.
		objects.push_back(o);
	}
    
    
    m_window = window; // Inicializamos a la ventana de visualización.
    m_outputWidth = std::max(width, 1); // Ancho y alto
    m_outputHeight = std::max(height, 1);
    m_outputRotation = rotation; // Rotación.

    CreateDevice(); // Creamos el dispotivo
    CreateResources(); // Creamos recursos que dependen del tamaño de la ventana de visualización

    CreateMainInputFlowResources(m_mesh); //Creamos recursos y objetos D3D12 que permiten el flujo de entrada de datos al pipeline
    LoadPrecompiledShaders(); // Cargamos shaders precompilados

    PSO(); // Creamos un estado del pipeline básico.

}



// Executes the basic game loop.
void Game::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}


void Game::Update(DX::StepTimer const& timer)
{
    float elapsedTime = float(timer.GetElapsedSeconds());

    // TODO: Actualización de las transformaciones en la escena

    float x = 0.0;
    float y = 0.0;
    float z = -30;

    // Recalculamos la matriz de vista
    XMVECTOR location = XMVectorSet(x, y, z, 1.0f);
    XMVECTOR target = XMVectorSet(0.0, 0.0, 1.0, 1.0f);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(location, target, up);
    XMMATRIX projection = XMMatrixPerspectiveFovLH(0.25 * XM_PI, m_outputWidth / m_outputHeight, 0.5f, 1000.0f);
    XMStoreFloat4x4(&m_view, view);

    static float delta = 0.0;
    delta += elapsedTime * (0.1 * XM_2PI);
    //void* data=nullptr;

    // upload de las constantes
    BYTE* data;

    m_vConstantBuffer[m_backBufferIndex]->Map(0, nullptr, reinterpret_cast<void**>(&data)); // realizamos el mapeo
    auto elementSizeConstants= CalcConstantBufferByteSize(sizeof(vConstants));
    XMFLOAT4X4 passTransform;
    XMStoreFloat4x4(&passTransform, XMMatrixIdentity());
    memcpy(data, reinterpret_cast<const void*>(&passTransform), sizeof(vConstants));
    if (m_vConstantBuffer[m_backBufferIndex] != nullptr)
        m_vConstantBuffer[m_backBufferIndex]->Unmap(0, nullptr);

    // upload del buffer estructurado
    m_vInstanceBuffer[m_backBufferIndex]->Map(0, nullptr, reinterpret_cast<void**>(&data)); // realizamos el mapeo
    auto elementSizeInstance = sizeof(vInstance);
    UINT count = 0;

	for (int i = 0; i < numInstances; i++){
		ObjectData &obj = objects.at(i);

        XMMATRIX world = XMLoadFloat4x4(&obj.m_world);
        XMMATRIX rotation = XMMatrixRotationZ(delta);

		// Los límites del objeto.
		if (obj.m_pos.x < -worldBoundaries.x || obj.m_pos.x > worldBoundaries.x) {
			obj.m_velocity.x *= -1;
		}

		if (obj.m_pos.y < -worldBoundaries.y || obj.m_pos.y > worldBoundaries.y) {
			obj.m_velocity.y *= -1;
		}

		if (obj.m_pos.z < -worldBoundaries.z || obj.m_pos.z > worldBoundaries.z) {
			obj.m_velocity.z *= -1;
		}
		
		// Para hacer pruebas por si se quiere debuggear pasado el tiempo.
		/*
		if (delta > 1) {
		}
		*/

		// Actualizar el movimiento de cada objeto según su posición y velocidad.
		XMFLOAT3 nextPos = XMFLOAT3(obj.m_pos.x + obj.m_velocity.x * obj.m_speed, obj.m_pos.y + obj.m_velocity.y * obj.m_speed, obj.m_pos.z + obj.m_velocity.z * obj.m_speed);
		obj.m_pos = nextPos;
        XMMATRIX translation = XMMatrixTranslation(obj.m_pos.x, obj.m_pos.y, obj.m_pos.z);

        world = world * rotation * translation;
        
        XMMATRIX worldview = world * view;
        XMMATRIX transform = worldview * projection;
		
        XMMATRIX normaltransform = XMMatrixTranspose(XMMatrixInverse(nullptr, worldview));
        XMStoreFloat4x4(&m_vInstances[m_backBufferIndex].NormalTransform, XMMatrixTranspose(normaltransform));
        XMStoreFloat4x4(&m_vInstances[m_backBufferIndex].Transform, XMMatrixTranspose(transform));
        m_vInstances[m_backBufferIndex].MaterialIndex = objects.at(i).matind;
        memcpy(&data[count*elementSizeInstance], reinterpret_cast<const void*>(&m_vInstances[m_backBufferIndex]), sizeof(vInstance)); //Copia de la transformación
        ++count;
    }

    
    if (m_vInstanceBuffer[m_backBufferIndex] != nullptr)
        m_vInstanceBuffer[m_backBufferIndex]->Unmap(0, nullptr);

		

    elapsedTime;
}


void Game::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    // Prepare the command list to render a new frame.
    Clear();

    // TODO: Add your rendering code here.
    CD3DX12_GPU_DESCRIPTOR_HANDLE cHandle(m_cDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    cHandle.Offset(2*m_backBufferIndex , m_cDescriptorSize);
    m_commandList->SetGraphicsRootDescriptorTable(0, // número de root parameter
            cHandle); //manejador a la posición del heap donde comenzaría el rango de descriptores
    cHandle.Offset(1, m_cDescriptorSize);
    m_commandList->SetGraphicsRootDescriptorTable(1, // número de root parameter
        cHandle);
    m_commandList->DrawIndexedInstanced(m_mesh.indices.size(), objects.size(), 0, 0, 0);
    
    // Show the new frame.
    Present();
}
// Helper method to prepare the command list for rendering and clear the back buffers.
void Game::Clear()
{
    // Reset command list and allocator.
    DX::ThrowIfFailed(m_commandAllocators[m_backBufferIndex]->Reset());
    DX::ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_backBufferIndex].Get(), m_pso.Get())); // Nota: establecer el PSO.

    // Transition the render target into the correct state to allow for drawing into it.
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_backBufferIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &barrier);

    // Clear the views.
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_backBufferIndex, m_rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvDescriptor(m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    m_commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    m_commandList->ClearRenderTargetView(rtvDescriptor, Colors::CornflowerBlue, 0, nullptr);
    m_commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set the viewport and scissor rect.
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(m_outputWidth), static_cast<float>(m_outputHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
    D3D12_RECT scissorRect = { 0, 0, m_outputWidth, m_outputHeight };
    m_commandList->RSSetViewports(1, &viewport);
    m_commandList->RSSetScissorRects(1, &scissorRect);

    /*  Establecemos en el pipeline la root signauture:
    La utilización de la root signature lleva tres acciones en la lista de comandos:
    a) Establecer la root signature
    b) Establecer un array de heaps de descriptores: en este caso el root parameter 0, es un descriptor table.
    Esto es un rango de descriptores, procedentes de un heap de descriptores.
    c) Establecer el punto de comienzo del rango de descriptores a usar (se referencia a partir del heap anterior)

    La lista de comandos que haga el Draw debe establecer la root signature
    */
    // Subtarea a: Establecer la root signature
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    // Subtarea b: b Establecer un array de descriptor heaps
    ID3D12DescriptorHeap* arrayHeaps[] = { m_cDescriptorHeap.Get(), m_sDescriptorHeap.Get()};
    m_commandList->SetDescriptorHeaps(_countof(arrayHeaps), arrayHeaps);

    // Subtarea c Establece el punto de comienzo del rango de descriptores.

   
    CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(m_cDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    srvHandle.Offset(2*c_swapBufferCount, m_cDescriptorSize);
    m_commandList->SetGraphicsRootDescriptorTable(2, // para SRV Textura
        srvHandle);

    CD3DX12_GPU_DESCRIPTOR_HANDLE sHandle(m_sDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    m_commandList->SetGraphicsRootDescriptorTable(3, // número de root parameter
        sHandle); //manejador a la posición del heap donde comenzaría el rango de descriptores
    
     // Todo: Establecemos la vista para el buffer de indices

    D3D12_VERTEX_BUFFER_VIEW aVBufferView[1] = { m_vBufferView }; // Es necesario pasar un array de buffer views

    m_commandList->IASetVertexBuffers(0, 1, aVBufferView);

    D3D12_INDEX_BUFFER_VIEW aIBufferView[1] = { m_iBufferView }; // Es necesario pasar un array de buffer views

    m_commandList->IASetIndexBuffer(aIBufferView);


    /* Establecemos la topología: es obligatorio*/
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);




}

// Submits the command list to the GPU and presents the back buffer contents to the screen.
void Game::Present()
{
    // Transition the render target to the state that allows it to be presented to the display.
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_backBufferIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1, &barrier);

    // Send the command list off to the GPU for processing.
    DX::ThrowIfFailed(m_commandList->Close());
    m_commandQueue->ExecuteCommandLists(1, CommandListCast(m_commandList.GetAddressOf()));

    // The first argument instructs DXGI to block until VSync, putting the application
    // to sleep until the next VSync. This ensures we don't waste any cycles rendering
    // frames that will never be displayed to the screen.
    HRESULT hr = m_swapChain->Present(1, 0);

    // If the device was reset we must completely reinitialize the renderer.
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        OnDeviceLost();
    }
    else
    {
        DX::ThrowIfFailed(hr);

        MoveToNextFrame();
    }
}

// Message handlers
void Game::OnActivated()
{
    // TODO: Game is becoming active window.
}

void Game::OnDeactivated()
{
    // TODO: Game is becoming background window.
}

void Game::OnSuspending()
{
    // TODO: Game is being power-suspended.
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();

    // TODO: Game is being power-resumed.
}

void Game::OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation)
{
    m_outputWidth = std::max(width, 1);
    m_outputHeight = std::max(height, 1);
    m_outputRotation = rotation;

    CreateResources();

    // TODO: Game window is being resized.
}

void Game::ValidateDevice()
{
    // The D3D Device is no longer valid if the default adapter changed since the device
    // was created or if the device has been removed.

    DXGI_ADAPTER_DESC previousDesc;
    {
        ComPtr<IDXGIAdapter1> previousDefaultAdapter;
        DX::ThrowIfFailed(m_dxgiFactory->EnumAdapters1(0, previousDefaultAdapter.GetAddressOf()));

        DX::ThrowIfFailed(previousDefaultAdapter->GetDesc(&previousDesc));
    }

    DXGI_ADAPTER_DESC currentDesc;
    {
        ComPtr<IDXGIFactory4> currentFactory;
        DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(currentFactory.GetAddressOf())));

        ComPtr<IDXGIAdapter1> currentDefaultAdapter;
        DX::ThrowIfFailed(currentFactory->EnumAdapters1(0, currentDefaultAdapter.GetAddressOf()));

        DX::ThrowIfFailed(currentDefaultAdapter->GetDesc(&currentDesc));
    }

    // If the adapter LUIDs don't match, or if the device reports that it has been removed,
    // a new D3D device must be created.

    if (previousDesc.AdapterLuid.LowPart != currentDesc.AdapterLuid.LowPart
        || previousDesc.AdapterLuid.HighPart != currentDesc.AdapterLuid.HighPart
        || FAILED(m_d3dDevice->GetDeviceRemovedReason()))
    {
        // Create a new device and swap chain.
        OnDeviceLost();
    }
}

// Properties
void Game::GetDefaultSize(int& width, int& height) const
{
    // TODO: Change to desired default window size (note minimum size is 320x200).
    width = 800;
    height = 600;
}

// These are the resources that depend on the device.
void Game::CreateDevice()
{
#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    //
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()))))
        {
            debugController->EnableDebugLayer();
        }
    }
#endif

    DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(m_dxgiFactory.ReleaseAndGetAddressOf())));

    ComPtr<IDXGIAdapter1> adapter;
    GetAdapter(adapter.GetAddressOf());

    // Create the DX12 API device object.
    DX::ThrowIfFailed(D3D12CreateDevice(
        adapter.Get(),
        m_featureLevel,
        IID_PPV_ARGS(m_d3dDevice.ReleaseAndGetAddressOf())
        ));

#ifndef NDEBUG
    // Configure debug device (if active).
    ComPtr<ID3D12InfoQueue> d3dInfoQueue;
    if (SUCCEEDED(m_d3dDevice.As(&d3dInfoQueue)))
    {
#ifdef _DEBUG
        d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
#endif
        D3D12_MESSAGE_ID hide[] =
        {
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE
        };
        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = _countof(hide);
        filter.DenyList.pIDList = hide;
        d3dInfoQueue->AddStorageFilterEntries(&filter);
    }
#endif

    // Create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    DX::ThrowIfFailed(m_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_commandQueue.ReleaseAndGetAddressOf())));

    // Create descriptor heaps for render target views and depth stencil views.
    D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {};
    rtvDescriptorHeapDesc.NumDescriptors = c_swapBufferCount;
    rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc = {};
    dsvDescriptorHeapDesc.NumDescriptors = 1;
    dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

    DX::ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(m_rtvDescriptorHeap.ReleaseAndGetAddressOf())));
    DX::ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(m_dsvDescriptorHeap.ReleaseAndGetAddressOf())));

    m_rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Create a command allocator for each back buffer that will be rendered to.
    for (UINT n = 0; n < c_swapBufferCount; n++)
    {
        DX::ThrowIfFailed(m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_commandAllocators[n].ReleaseAndGetAddressOf())));
    }

    // Create a command list for recording graphics commands.
    DX::ThrowIfFailed(m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(m_commandList.ReleaseAndGetAddressOf())));
    
    //DX::ThrowIfFailed(m_commandList->Close());

    // Create a fence for tracking GPU execution progress.
    DX::ThrowIfFailed(m_d3dDevice->CreateFence(m_fenceValues[m_backBufferIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.ReleaseAndGetAddressOf())));
    m_fenceValues[m_backBufferIndex]++;

    m_fenceEvent.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
    if (!m_fenceEvent.IsValid())
    {
        throw std::exception("CreateEvent");
    }

    // TODO: Initialize device dependent objects here (independent of window size).
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateResources()
{
    // Wait until all previous GPU work is complete.
    WaitForGpu();

    // Release resources that are tied to the swap chain and update fence values.
    for (UINT n = 0; n < c_swapBufferCount; n++)
    {
        m_renderTargets[n].Reset();
        m_fenceValues[n] = m_fenceValues[m_backBufferIndex];
    }

    DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;
    UINT backBufferWidth = static_cast<UINT>(m_outputWidth);
    UINT backBufferHeight = static_cast<UINT>(m_outputHeight);

    // If the swap chain already exists, resize it, otherwise create one.
    if (m_swapChain)
    {
        HRESULT hr = m_swapChain->ResizeBuffers(c_swapBufferCount, backBufferWidth, backBufferHeight, backBufferFormat, 0);

        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
            // If the device was removed for any reason, a new device and swap chain will need to be created.
            OnDeviceLost();

            // Everything is set up now. Do not continue execution of this method. OnDeviceLost will reenter this method
            // and correctly set up the new device.
            return;
        }
        else
        {
            DX::ThrowIfFailed(hr);
        }
    }
    else
    {
        // Create a descriptor for the swap chain.
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = backBufferWidth;
        swapChainDesc.Height = backBufferHeight;
        swapChainDesc.Format = backBufferFormat;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = c_swapBufferCount;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Scaling = DXGI_SCALING_ASPECT_RATIO_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

        // Create a swap chain for the window.
        ComPtr<IDXGISwapChain1> swapChain;
        DX::ThrowIfFailed(m_dxgiFactory->CreateSwapChainForCoreWindow(
            m_commandQueue.Get(),
            m_window,
            &swapChainDesc,
            nullptr,
            swapChain.GetAddressOf()
            ));

        DX::ThrowIfFailed(swapChain.As(&m_swapChain));
    }

    DX::ThrowIfFailed(m_swapChain->SetRotation(m_outputRotation));

    // Obtain the back buffers for this window which will be the final render targets
    // and create render target views for each of them.
    for (UINT n = 0; n < c_swapBufferCount; n++)
    {
        DX::ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(m_renderTargets[n].GetAddressOf())));

        wchar_t name[25] = {};
        swprintf_s(name, L"Render target %u", n);
        m_renderTargets[n]->SetName(name);

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(
            m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            static_cast<INT>(n), m_rtvDescriptorSize);
        m_d3dDevice->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvDescriptor);
    }

    // Reset the index to the current back buffer.
    m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Allocate a 2-D surface as the depth/stencil buffer and create a depth/stencil view
    // on this surface.
    CD3DX12_HEAP_PROPERTIES depthHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        depthBufferFormat,
        backBufferWidth,
        backBufferHeight,
        1, // This depth stencil view has only one texture.
        1  // Use a single mipmap level.
        );
    depthStencilDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
    depthOptimizedClearValue.Format = depthBufferFormat;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;

    DX::ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &depthHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthOptimizedClearValue,
        IID_PPV_ARGS(m_depthStencil.ReleaseAndGetAddressOf())
        ));

    m_depthStencil->SetName(L"Depth stencil");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = depthBufferFormat;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    m_d3dDevice->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc, m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    // TODO: Initialize windows-size dependent objects here.
}

void Game::WaitForGpu()
{
    // Schedule a Signal command in the GPU queue.
    DX::ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_backBufferIndex]));

    // Wait until the Signal has been processed.
    DX::ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_backBufferIndex], m_fenceEvent.Get()));
    WaitForSingleObjectEx(m_fenceEvent.Get(), INFINITE, FALSE);

    // Increment the fence value for the current frame.
    m_fenceValues[m_backBufferIndex]++;
}

void Game::MoveToNextFrame()
{
    // Schedule a Signal command in the queue.
    const UINT64 currentFenceValue = m_fenceValues[m_backBufferIndex];
    DX::ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

    // Update the back buffer index.
    m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    // If the next frame is not ready to be rendered yet, wait until it is ready.
    if (m_fence->GetCompletedValue() < m_fenceValues[m_backBufferIndex])
    {
        DX::ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_backBufferIndex], m_fenceEvent.Get()));
        WaitForSingleObjectEx(m_fenceEvent.Get(), INFINITE, FALSE);
    }

    // Set the fence value for the next frame.
    m_fenceValues[m_backBufferIndex] = currentFenceValue + 1;
}

// This method acquires the first available hardware adapter that supports Direct3D 12.
// If no such adapter can be found, try WARP. Otherwise throw an exception.
void Game::GetAdapter(IDXGIAdapter1** ppAdapter)
{
    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != m_dxgiFactory->EnumAdapters1(adapterIndex, adapter.ReleaseAndGetAddressOf()); ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc;
        DX::ThrowIfFailed(adapter->GetDesc1(&desc));

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            // Don't select the Basic Render Driver adapter.
            continue;
        }

        // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), m_featureLevel, _uuidof(ID3D12Device), nullptr)))
        {
            break;
        }
    }

#if !defined(NDEBUG)
    if (!adapter)
    {
        if (FAILED(m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()))))
        {
            throw std::exception("WARP12 not available. Enable the 'Graphics Tools' optional feature");
        }
    }
#endif

    if (!adapter)
    {
        throw std::exception("No Direct3D 12 device found");
    }

    *ppAdapter = adapter.Detach();
}

void Game::OnDeviceLost()
{
    // TODO: Perform Direct3D resource cleanup.

    for (UINT n = 0; n < c_swapBufferCount; n++)
    {
        m_commandAllocators[n].Reset();
        m_renderTargets[n].Reset();
    }

    m_depthStencil.Reset();
    m_fence.Reset();
    m_commandList.Reset();
    m_swapChain.Reset();
    m_rtvDescriptorHeap.Reset();
    m_dsvDescriptorHeap.Reset();
    m_commandQueue.Reset();
    m_d3dDevice.Reset();
    m_dxgiFactory.Reset();

    CreateDevice();
    CreateResources();
}

void Game::CreateMainInputFlowResources(const Mesh& mesh) {

    /*
    Objetivo 1.
    Comenzamos por preparar los buffers de vértices y de índices. Estos buffers se van a cargar
    con datos al principio, y luego no se van a actualizar por cada frame. Por eso es mejor
    crearlos en un heap de tipo DEFAULT al que accede con mayor eficiencia la GPU. Por contra
    la CPU no puede cargar datos en un DEFAULT. Entonces hay que crear un segundo buffer en un heap
    de tipo UPLOAD y transferir del UPLOAD al DEFAULT. Los buffer de vértices e índices no requieren
    un heap de descriptores, se conectan al pipeline directamente con una vista.

    */

    /*Tarea 1: Creación de los buffers.*/

    // Creación de los buffers default y upload para los vértices
    DX::ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(mesh.GetVSize()),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(m_vBufferDefault.GetAddressOf())));

    // Ahora un buffer upload, de esta forma tenemos un puente upload para pasar a default.

    DX::ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(mesh.GetVSize()),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_vBufferUpload.GetAddressOf())));

    // Creación de los buffers default y upload para los índices
    DX::ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(mesh.GetISize()),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(m_iBufferDefault.GetAddressOf())));

    // Ahora un buffer upload, de esta forma tenemos un puente upload para pasar a default.

    DX::ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(mesh.GetISize()),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_iBufferUpload.GetAddressOf())));

    /*Tarea 2: Preparamos el origen de los datos de vértices e índices.*/

    // Preparamos el  origen_vertices
    D3D12_SUBRESOURCE_DATA origen_vertices = {};
    origen_vertices.pData = &mesh.vertices[0];
    origen_vertices.RowPitch = mesh.GetVSize();
    origen_vertices.SlicePitch = origen_vertices.RowPitch;

    D3D12_SUBRESOURCE_DATA origen_indices = {};
    origen_indices.pData = &mesh.indices[0];
    origen_indices.RowPitch = mesh.GetISize();
    origen_indices.SlicePitch = origen_indices.RowPitch;

    /*Tarea 3: Realizamos la transferencia desde el origen hasta el buffer DEFAULT pasando por el buffer UPLOAD*/
    /*Vértices*/
    // Cambio de estado en el recurso de destino
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_vBufferDefault.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));

    // Ordenamos la transferencia vertices
    UpdateSubresources<1>(m_commandList.Get(), m_vBufferDefault.Get(), m_vBufferUpload.Get(), 0, 0, 1, &origen_vertices);
    // Cambio de estado en el recurso de destino
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_vBufferDefault.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

    /*Índices*/
    // Cambio de estado en el recurso de destino
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_iBufferDefault.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));

    // Ordenamos la transferencia vertices
    UpdateSubresources<1>(m_commandList.Get(), m_iBufferDefault.Get(), m_iBufferUpload.Get(), 0, 0, 1, &origen_indices);
    // Cambio de estado en el recurso de destino
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_iBufferDefault.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));


    /* Tarea 3: Establecemos una vista para vértices e índices*/
    // Establecemos la vista (descriptor) para el buffer de vértices
    /* Vértices*/

    m_vBufferView.BufferLocation = m_vBufferDefault->GetGPUVirtualAddress();
    m_vBufferView.StrideInBytes = sizeof(Vertex);
    m_vBufferView.SizeInBytes = mesh.GetVSize();



    D3D12_VERTEX_BUFFER_VIEW aVBufferView[1] = { m_vBufferView }; // Es necesario pasar un array de buffer views

    m_commandList->IASetVertexBuffers(0, 1, aVBufferView);

    /* Índices*/

    m_iBufferView.BufferLocation = m_iBufferDefault->GetGPUVirtualAddress();
    m_iBufferView.Format = DXGI_FORMAT_R32_UINT;
    m_iBufferView.SizeInBytes = mesh.GetISize();

    // Todo: Establecemos la vista para el buffer de indices

    D3D12_INDEX_BUFFER_VIEW aIBufferView[1] = { m_iBufferView }; // Es necesario pasar un array de buffer views

    m_commandList->IASetIndexBuffer(aIBufferView);

    /* Tarea 4: Cargamos la textura de la malla*/

    m_mesh.meshTexture = std::make_unique<Mesh::Texture>();
    m_mesh.meshTexture->Name = "tutorialTex";
    m_mesh.meshTexture->Filename = L"tutorialtextura.dds";
    DX::ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_d3dDevice.Get(), m_commandList.Get(),
        m_mesh.meshTexture->Filename.c_str(),
        m_mesh.meshTexture->Resource,
        m_mesh.meshTexture->UploadHeap));
        
    /*------------------------- Fin Objetivo 1  ----------------------------------------------------------------------------*/


    unsigned int elementSizeConstants = CalcConstantBufferByteSize(sizeof(vConstants));

    for (int i = 0; i < c_swapBufferCount; i++) {
        m_d3dDevice->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(elementSizeConstants),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(m_vConstantBuffer[i].GetAddressOf())
        );
    }

    unsigned int elementSizeInstance = sizeof(vInstance);

    for (int i = 0; i < c_swapBufferCount; i++) {
        m_d3dDevice->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(elementSizeInstance*objects.size()),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(m_vInstanceBuffer[i].GetAddressOf())
        );
    }

    /* Tarea 2: crear un heap de descriptores CBV_SRV_UAV*/

    D3D12_DESCRIPTOR_HEAP_DESC cHeapDescriptor;
    cHeapDescriptor.NumDescriptors = 2*c_swapBufferCount+1; // (CBV(Pass) + SRV(Instance)) por swap buffer + textura
    cHeapDescriptor.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cHeapDescriptor.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cHeapDescriptor.NodeMask = 0;

    m_d3dDevice->CreateDescriptorHeap(&cHeapDescriptor, IID_PPV_ARGS(&m_cDescriptorHeap));

    /* Tarea 3: Crear un heap de descriptores para samplers*/
    D3D12_DESCRIPTOR_HEAP_DESC descHeapSampler = {};
    descHeapSampler.NumDescriptors = 1;
    descHeapSampler.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    descHeapSampler.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    DX::ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&descHeapSampler, IID_PPV_ARGS(&m_sDescriptorHeap)));


   
    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(
        m_cDescriptorHeap->GetCPUDescriptorHandleForHeapStart()
    );

    m_cDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    
    for (int i = 0; i < c_swapBufferCount; i++) {
        D3D12_GPU_VIRTUAL_ADDRESS cAddress = m_vConstantBuffer[i]->GetGPUVirtualAddress();
        D3D12_CONSTANT_BUFFER_VIEW_DESC cDescriptor;
        cDescriptor.BufferLocation = cAddress;
        cDescriptor.SizeInBytes = elementSizeConstants;
        m_d3dDevice->CreateConstantBufferView(&cDescriptor, hDescriptor);
        hDescriptor.Offset(1, m_cDescriptorSize);
        cAddress = m_vInstanceBuffer[i]->GetGPUVirtualAddress();
        D3D12_SHADER_RESOURCE_VIEW_DESC sBDesc = {};
        sBDesc = {};
        sBDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sBDesc.Format = m_vInstanceBuffer[i].Get()->GetDesc().Format;
        sBDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        sBDesc.Buffer.FirstElement = 0;
        sBDesc.Buffer.NumElements = objects.size();
        sBDesc.Buffer.StructureByteStride = sizeof(vInstance);
        sBDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        
        m_d3dDevice->CreateShaderResourceView(m_vInstanceBuffer[i].Get(),&sBDesc, hDescriptor);
        hDescriptor.Offset(1, m_cDescriptorSize);
           
    }
    /* Tarea 5 Creamos un descriptor SRV para la textura*/
    // Obtenemos un handle para el descriptor.
    
    
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = m_mesh.meshTexture->Resource->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    //srvDesc.Texture2D.MipLevels = m_mesh.meshTexture->Resource->GetDesc().MipLevels;
    srvDesc.Texture2D.MipLevels = -1;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    m_d3dDevice->CreateShaderResourceView(m_mesh.meshTexture->Resource.Get(), &srvDesc, hDescriptor);

    /* Tarea 6 Creamoes un descriptor para el sampler*/
    D3D12_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    m_d3dDevice->CreateSampler(&samplerDesc, m_sDescriptorHeap->GetCPUDescriptorHandleForHeapStart());






    /*
    Objetivo 3: Creamos una root signature ya que usaremos registros de los shaders conectados a recursos y
    la configuramos para ser usada por el pipeline
    */
    // Establecemos la root signature

/* Tarea 1: Crear un array de root parameters*/

    CD3DX12_ROOT_PARAMETER rootParameters[4]; //Array de root parameters // CBT
    // Creamos un rango de tablas de descriptores
    CD3DX12_DESCRIPTOR_RANGE descRange[4]; // CBT
    descRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); //1 CB to slot 0
    descRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 1); //1 SRV for SB to Slot 0, space 1 f
    descRange[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // 1 SRV to Slot 0, space 0 for texture
    descRange[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0); // 1 Sampler to Slot 0


    rootParameters[0].InitAsDescriptorTable(1, // Número de rangos 
        &descRange[0],D3D12_SHADER_VISIBILITY_ALL); 
    rootParameters[1].InitAsDescriptorTable(1, // Número de rangos 
        &descRange[1], D3D12_SHADER_VISIBILITY_ALL);
    rootParameters[2].InitAsDescriptorTable(1, // Número de rangos 
        &descRange[2], D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[3].InitAsDescriptorTable(1, // Número de rangos 
        &descRange[3], D3D12_SHADER_VISIBILITY_PIXEL);

/* Tarea 2: Creamos una descripción de la root signature y la serializamos */
    // Descripción de la root signature //CBT
    CD3DX12_ROOT_SIGNATURE_DESC rsDescription(4, rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    //Debemos serializar la descripción root signature
    Microsoft::WRL::ComPtr<ID3DBlob> serializado = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> error = nullptr;
    D3D12SerializeRootSignature(&rsDescription, D3D_ROOT_SIGNATURE_VERSION_1, serializado.GetAddressOf(), error.GetAddressOf());

    /* Tarea 3: Creamos la root signature*/
        //Con la descripción serializada creamos el componente ID3DRootSignature
    DX::ThrowIfFailed(m_d3dDevice->CreateRootSignature(
        0,
        serializado->GetBufferPointer(),
        serializado->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignature)));



    // Cerramos la lista de comandos, lanzamos la ejeución de los mismos

    m_commandList->Close();


    m_commandQueue->ExecuteCommandLists(1, CommandListCast(m_commandList.GetAddressOf()));


    DX::ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), 1));

    // Wait until the Signal has been processed.
    DX::ThrowIfFailed(m_fence->SetEventOnCompletion(1, m_fenceEvent.Get()));
    WaitForSingleObjectEx(m_fenceEvent.Get(), INFINITE, FALSE);




}

void Game::LoadPrecompiledShaders() {

    DX::ThrowIfFailed(
        D3DReadFileToBlob(L"vertex.cso", m_vsByteCode.GetAddressOf()));

    DX::ThrowIfFailed(
        D3DReadFileToBlob(L"pixel.cso", m_psByteCode.GetAddressOf()));


    m_vs = { reinterpret_cast<char*>(m_vsByteCode->GetBufferPointer()),
            m_vsByteCode->GetBufferSize() };

    m_ps = { reinterpret_cast<char*>(m_psByteCode->GetBufferPointer()),
            m_psByteCode->GetBufferSize() };

}

void Game::PSO()
{

    // Input Layout


    m_inputLayout = {

        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
    {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,28,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
    {"UV",0,DXGI_FORMAT_R32G32_FLOAT,0,40,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
    };



    // Rasterizer state
    CD3DX12_RASTERIZER_DESC rasterizer(D3D12_DEFAULT);
    rasterizer.FillMode = D3D12_FILL_MODE_WIREFRAME;
    rasterizer.CullMode = D3D12_CULL_MODE_NONE;

    ZeroMemory(&m_psoDescriptor, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));


    m_psoDescriptor.InputLayout = { m_inputLayout.data(), (unsigned int)m_inputLayout.size() };
    m_psoDescriptor.pRootSignature = m_rootSignature.Get();
    m_psoDescriptor.VS = { m_vs.pShaderBytecode,m_vs.BytecodeLength };
    m_psoDescriptor.PS = { m_ps.pShaderBytecode,m_ps.BytecodeLength };
    m_psoDescriptor.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    //m_psoDescriptor.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    //m_psoDescriptor.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    m_psoDescriptor.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    m_psoDescriptor.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    m_psoDescriptor.SampleMask = UINT_MAX;
    m_psoDescriptor.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    m_psoDescriptor.NumRenderTargets = 1;
    m_psoDescriptor.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    m_psoDescriptor.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    m_psoDescriptor.SampleDesc.Count = 1;
    m_psoDescriptor.SampleDesc.Quality = 0;

    m_d3dDevice->CreateGraphicsPipelineState(&m_psoDescriptor, IID_PPV_ARGS(&m_pso));



}

