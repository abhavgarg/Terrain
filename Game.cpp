//
// Game.cpp
//

#include "pch.h"
#include "Game.h"


//toreorganise
#include <fstream>

extern void ExitGame();

using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace ImGui;

using Microsoft::WRL::ComPtr;

Game::Game() noexcept(false)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);
}

Game::~Game()
{
#ifdef DXTK_AUDIO
    if (m_audEngine)
    {
        m_audEngine->Suspend();
    }
#endif
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{

	m_input.Initialise(window);

    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
    auto device = m_deviceResources->GetD3DDevice();
    auto context = m_deviceResources->GetD3DDeviceContext();

	//setup imgui.  its up here cos we need the window handle too
	//pulled from imgui directx11 example
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(window);		//tie to our window
	ImGui_ImplDX11_Init(device, context);	//tie to directx

	m_fullscreenRect.left = 0;
	m_fullscreenRect.top = 0;
	m_fullscreenRect.right = 800;
	m_fullscreenRect.bottom = 600;

	m_CameraViewRect.left = 500;
	m_CameraViewRect.top = 0;
	m_CameraViewRect.right = 800;
	m_CameraViewRect.bottom = 240;

	//setup light
	m_Light.setAmbientColour(0.3f, 0.3f, 0.3f, 1.0f);
	m_Light.setDiffuseColour(1.0f, 1.0f, 1.0f, 1.0f);
	m_Light.setPosition(1.0f, 1.0f, 1.0f);
	m_Light.setDirection(-1.0f, -1.0f, 0.0f);


	//setup camera
	m_Camera01.setPosition(Vector3(10.0f, 2.0f, -10.0f));
	m_Camera01.setRotation(Vector3(-90.0f, -180.0f, 0.0f));	//orientation is -90 becuase zero will be looking up at the sky straight up. 

    m_postProcess = std::make_unique<BasicPostProcess>(device);


    CD3D11_TEXTURE2D_DESC sceneDesc(
        DXGI_FORMAT_R16G16B16A16_FLOAT, width, height,
        1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);

    DX::ThrowIfFailed(
        device->CreateTexture2D(&sceneDesc, nullptr, m_sceneTex.GetAddressOf())
    );

    DX::ThrowIfFailed(
        device->CreateShaderResourceView(m_sceneTex.Get(), nullptr,
            m_sceneSRV.ReleaseAndGetAddressOf())
    );

    DX::ThrowIfFailed(
        device->CreateRenderTargetView(m_sceneTex.Get(), nullptr,
            m_sceneRT.ReleaseAndGetAddressOf()
        ));

#ifdef DXTK_AUDIO
    // Create DirectXTK for Audio objects
    AUDIO_ENGINE_FLAGS eflags = AudioEngine_Default;
#ifdef _DEBUG
    eflags = eflags | AudioEngine_Debug;
#endif

    m_audEngine = std::make_unique<AudioEngine>(eflags);

    m_audioEvent = 0;
    m_audioTimerAcc = 10.f;
    m_retryDefault = false;

    m_waveBank = std::make_unique<WaveBank>(m_audEngine.get(), L"adpcmdroid.xwb");

    m_soundEffect = std::make_unique<SoundEffect>(m_audEngine.get(), L"MusicMono_adpcm.wav");
    m_effect1 = m_soundEffect->CreateInstance();
    m_effect2 = m_waveBank->CreateInstance(10);

    m_effect1->Play(true);
    m_effect2->Play();
#endif
}

#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
	//take in input
	m_input.Update();								//update the hardware
	m_gameInputCommands = m_input.getGameInput();	//retrieve the input for our game
	
	//Update all game objects
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

	//Render all game content. 
    Render();

#ifdef DXTK_AUDIO
    // Only update audio engine once per frame
    if (!m_audEngine->IsCriticalError() && m_audEngine->Update())
    {
        // Setup a retry in 1 second
        m_audioTimerAcc = 1.f;
        m_retryDefault = true;
    }
#endif

	
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{	
	//this is hacky,  i dont like this here.  
	auto device = m_deviceResources->GetD3DDevice();

	//note that currently.  Delta-time is not considered in the game object movement. 
    if (m_gameInputCommands.forward)
    {
        Vector3 position = m_Camera01.getPosition(); //get the position
        position += (m_Camera01.getForward() * m_Camera01.getMoveSpeed()); //add the forward vector
        m_Camera01.setPosition(position);
    }
    if (m_gameInputCommands.back)
    {
        Vector3 position = m_Camera01.getPosition(); //get the position
        position -= (m_Camera01.getForward() * m_Camera01.getMoveSpeed()); //add the forward vector
        m_Camera01.setPosition(position);
    }
    //&& m_Camera01.getPosition().x > 0 && m_Camera01.getPosition().x < 20 && m_Camera01.getPosition().z > 0 && m_Camera01.getPosition().z < 20

    if (m_gameInputCommands.left)
    {
        Vector3 position = m_Camera01.getPosition(); //get the position
        position -= (m_Camera01.getRight() * m_Camera01.getMoveSpeed());
        m_Camera01.setPosition(position);

    }
    if (m_gameInputCommands.right)
    {
        Vector3 position = m_Camera01.getPosition(); //get the position
        position += (m_Camera01.getRight() * m_Camera01.getMoveSpeed());
        m_Camera01.setPosition(position);
    }

    if (m_gameInputCommands.up && m_Camera01.getPosition().y < 5)
    {
        Vector3 position = m_Camera01.getPosition(); //get the position
        position.y += m_Camera01.getMoveSpeed(); //add height
        m_Camera01.setPosition(position);

    }
    if (m_gameInputCommands.down && m_Camera01.getPosition().y > 0)
    {
        Vector3 position = m_Camera01.getPosition(); //get the position
        position.y -= m_Camera01.getMoveSpeed();
        m_Camera01.setPosition(position);
    }

    if (m_gameInputCommands.rotLeft)
    {
        Vector3 rotation = m_Camera01.getRotation();
        rotation.y += m_Camera01.getRotationSpeed();
        m_Camera01.setRotation(rotation);
    }

    if (m_gameInputCommands.rotRight)
    {
        Vector3 rotation = m_Camera01.getRotation();
        rotation.y -= m_Camera01.getRotationSpeed();
        m_Camera01.setRotation(rotation);
    }

	if (m_gameInputCommands.randomheight) //Random Height Map
	{
		m_Terrain.RandomHeightMap(device);
	}

    if (m_gameInputCommands.fault) //Faulting
    {
        m_Terrain.Faulting(device);
    }

    if (m_gameInputCommands.smooth) //Smoothening
    {
        m_Terrain.SmoothenHeightMap(device, 1.5);
    }

    if (m_gameInputCommands.generate) //Generate Scene
    {
        m_Terrain.GenerateHeightMap(device);
    }

    if (m_gameInputCommands.noise) //2D Simplex Noise
    {
        m_Terrain.NoiseHeightMap(device);
    }
    if (m_gameInputCommands.pardep) //Particle Deposition
    {
        m_Terrain.ParticleDeposition(device);
    }

    if (m_gameInputCommands.post) //default filter
    {
        postProcessLoop = 0;
    }

    if (m_gameInputCommands.blur) //bloom blur
    {
        postProcessLoop = 1;
    }

    if (m_gameInputCommands.mono) //monochrome
    {
        postProcessLoop = 2;
    }

    if (m_gameInputCommands.sepi) //sepia
    {
        postProcessLoop = 3;
    }
    
	m_Camera01.Update();	//camera update.
	m_Terrain.Update();		//terrain update.

	m_view = m_Camera01.getCameraMatrix();
	m_world = Matrix::Identity;

	/*create our UI*/
	SetupGUI();

#ifdef DXTK_AUDIO
    m_audioTimerAcc -= (float)timer.GetElapsedSeconds();
    if (m_audioTimerAcc < 0)
    {
        if (m_retryDefault)
        {
            m_retryDefault = false;
            if (m_audEngine->Reset())
            {
                // Restart looping audio
                m_effect1->Play(true);
            }
        }
        else
        {
            m_audioTimerAcc = 4.f;

            m_waveBank->Play(m_audioEvent++);

            if (m_audioEvent >= 11)
                m_audioEvent = 0;
        }
    }
#endif

  
	if (m_input.Quit())
	{
		ExitGame();
	}
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{	
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    Clear();

    m_deviceResources->PIXBeginEvent(L"Render");
    auto device = m_deviceResources->GetD3DDevice();
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();
    context->ClearRenderTargetView(m_sceneRT.Get(), Colors::CornflowerBlue);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    context->OMSetRenderTargets(1, m_sceneRT.GetAddressOf(), depthStencil);

    // Render Terrain
    m_world = SimpleMath::Matrix::Identity; //set world back to identity
    SimpleMath::Matrix newPosition = Matrix::CreateScale(0.2f) * SimpleMath::Matrix::CreateTranslation(0, -2.0f, 0.f);
    SimpleMath::Matrix newRotation = SimpleMath::Matrix::CreateRotationX(XM_PI);
    m_world = m_world * newRotation * newPosition;
    m_BasicShaderPair.EnableShader(context);
    m_BasicShaderPair.SetShaderParameters(context, &m_world, &(Matrix)m_view, &(Matrix)m_projection, &m_Light, m_texturegrass.Get(), m_texturemountain.Get(), m_texturerock.Get(), m_texturesnow.Get());
    m_Terrain.Render(context);


    //Enter water terrain code here

    m_deviceResources->PIXEndEvent();

    context->OMSetRenderTargets(1, &renderTarget, nullptr);


    if (postProcessLoop == 0)
    {
        m_postProcess->SetEffect(BasicPostProcess::Copy);
        //m_timer 
    }
    if (postProcessLoop == 1)
    {
        m_postProcess->SetEffect(BasicPostProcess::BloomBlur);
    }
    if (postProcessLoop == 2)
    {
        m_postProcess->SetEffect(BasicPostProcess::Monochrome);
    }
    if (postProcessLoop == 3)
    {
        m_postProcess->SetEffect(BasicPostProcess::Sepia);
    }

    m_postProcess->SetSourceTexture(m_sceneSRV.Get());
    m_postProcess->Process(context); // THIS LINE IS THE PROBLEM

	//render our GUI
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	

    // Show the new frame.
    m_deviceResources->Present();
}


// Helper method to clear the back buffers.
void Game::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");

    // Clear the views.
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, Colors::CornflowerBlue);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // Set the viewport.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    m_deviceResources->PIXEndEvent();
}

#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnActivated()
{
}

void Game::OnDeactivated()
{
}

void Game::OnSuspending()
{
#ifdef DXTK_AUDIO
    m_audEngine->Suspend();
#endif
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();

#ifdef DXTK_AUDIO
    m_audEngine->Resume();
#endif
}

void Game::OnWindowMoved()
{
    auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Game::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();
}

#ifdef DXTK_AUDIO
void Game::NewAudioDevice()
{
    if (m_audEngine && !m_audEngine->IsAudioDevicePresent())
    {
        // Setup a retry in 1 second
        m_audioTimerAcc = 1.f;
        m_retryDefault = true;
    }
}
#endif

// Properties
void Game::GetDefaultSize(int& width, int& height) const
{
    width = 1920;
    height = 1080;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto device = m_deviceResources->GetD3DDevice();

	m_BasicShaderPair.InitStandard(device, L"light_vs.cso", L"light_ps.cso");

    CreateDDSTextureFromFile(device, L"grass.dds", nullptr, m_texturegrass.ReleaseAndGetAddressOf());
    CreateDDSTextureFromFile(device, L"mountain.dds", nullptr, m_texturemountain.ReleaseAndGetAddressOf());
    CreateDDSTextureFromFile(device, L"rock.dds", nullptr, m_texturerock.ReleaseAndGetAddressOf());
    CreateDDSTextureFromFile(device, L"water.dds", nullptr, m_texturewater.ReleaseAndGetAddressOf());
    CreateDDSTextureFromFile(device, L"snow.dds", nullptr, m_texturesnow.ReleaseAndGetAddressOf());
    m_Terrain.Initialize(device, 128, 128);
    m_Terrain.GenerateHeightMap(device);
    device;
    context;


}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    auto size = m_deviceResources->GetOutputSize();
    float aspectRatio = float(size.right) / float(size.bottom);
    float fovAngleY = 70.0f * XM_PI / 180.0f;

    // This is a simple example of change that can be made when the app is in
    // portrait or snapped view.
    if (aspectRatio < 1.0f)
    {
        fovAngleY *= 2.0f;
    }

    // This sample makes use of a right-handed coordinate system using row-major matrices.
    m_projection = Matrix::CreatePerspectiveFieldOfView(
        fovAngleY,
        aspectRatio,
        0.01f,
        100.0f
    );
}



void Game::SetupGUI()
{

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
    if (m_show_window)
    {
        ImGui::Begin("Controls", &m_show_window);
        ImGui::Text("Press up and down arrow keys for adjusting height");
        ImGui::Text("Press left and right arrow keys for y-axis rotation");
        ImGui::Text("Press WASD to move around scene");
        ImGui::Text("Press R to generate random height terrain");
        ImGui::Text("Press F to add Faulting");
        ImGui::Text("Press N to add Simplex Noise");
        ImGui::Text("Press U to add Smoothening");
        ImGui::Text("Press P to deposit a random particle");
        ImGui::Text("Press 0123 for different Post Processing effects");
        ImGui::Text("Camera Position X: %f", m_Camera01.getPosition().x);
        ImGui::Text("Camera Position Y: %f", m_Camera01.getPosition().y);
        ImGui::Text("Camera Position Z: %f", m_Camera01.getPosition().z);
        if (ImGui::Button("Close Me"))
            m_show_window = false;
        ImGui::End();
    }
}


void Game::OnDeviceLost()
{
    m_states.reset();
    m_fxFactory.reset();
    m_sprites.reset();
    m_font.reset();
	m_batch.reset();
	m_testmodel.reset();
    m_batchInputLayout.Reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}

#pragma endregion
