#include "pch.h"
#include "Input.h"


Input::Input()
{
}

Input::~Input()
{
}

void Input::Initialise(HWND window)
{
	m_keyboard = std::make_unique<DirectX::Keyboard>();
	m_mouse = std::make_unique<DirectX::Mouse>();
	m_mouse->SetWindow(window);
	m_quitApp = false;

	m_GameInput.forward		= false;
	m_GameInput.back		= false;
	m_GameInput.right		= false;
	m_GameInput.left		= false;
	m_GameInput.rotRight	= false;
	m_GameInput.rotLeft		= false;
}

void Input::Update()
{
	auto kb = m_keyboard->GetState();	//updates the basic keyboard state
	m_KeyboardTracker.Update(kb);		//updates the more feature filled state. Press / release etc. 
	auto mouse = m_mouse->GetState();   //updates the basic mouse state
	m_MouseTracker.Update(mouse);		//updates the more advanced mouse state. 

	if (kb.Escape)// check has escape been pressed.  if so, quit out. 
	{
		m_quitApp = true;
	}

	//left Aroow key
	if (kb.Left)	m_GameInput.rotLeft = true;
	else		m_GameInput.rotLeft = false;

	//right arrow key
	if (kb.Right)	m_GameInput.rotRight = true;
	else		m_GameInput.rotRight = false;

	//Up Arrow key
	if (kb.Up)	m_GameInput.up = true;
	else		m_GameInput.up = false;

	//Down Arrow key
	if (kb.Down)	m_GameInput.down = true;
	else		m_GameInput.down = false;

	//W key
	if (kb.W)	m_GameInput.forward = true;
	else		m_GameInput.forward = false;


	//S key
	if (kb.S)	m_GameInput.back = true;
	else		m_GameInput.back = false;

	//A key
	if (kb.A)	m_GameInput.left = true;
	else		m_GameInput.left = false;


	//D key
	if (kb.D)	m_GameInput.right = true;
	else		m_GameInput.right = false;


	//R Key
	if (kb.R) m_GameInput.randomheight = true;
	else		m_GameInput.randomheight = false;

	//F Key
	if (kb.F) m_GameInput.fault = true;
	else		m_GameInput.fault = false;

	//U Key
	if (kb.U) m_GameInput.smooth = true;
	else		m_GameInput.smooth = false;

	//Space Key
	if (kb.Space) m_GameInput.generate = true;
	else		m_GameInput.generate = false;

	//N Key
	if (kb.N) m_GameInput.noise = true;
	else		m_GameInput.noise = false;

	//P Key
	if (kb.P) m_GameInput.pardep = true;
	else		m_GameInput.pardep = false;

	//Numpad0 Key
	if (kb.NumPad0 || kb.D0) m_GameInput.post = true;
	else		m_GameInput.post = false;

	//Numpad1 Key
	if (kb.NumPad1 || kb.D1) m_GameInput.blur = true;
	else		m_GameInput.blur = false;

	//Numpad2 Key
	if (kb.NumPad2 || kb.D2) m_GameInput.mono = true;
	else		m_GameInput.mono = false;

	//Numpad3 Key
	if (kb.NumPad3 || kb.D3) m_GameInput.sepi = true;
	else		m_GameInput.sepi = false;


}

bool Input::Quit()
{
	return m_quitApp;
}

InputCommands Input::getGameInput()
{
	return m_GameInput;
}
