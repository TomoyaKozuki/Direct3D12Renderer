#include<Windows.h>
#include<WindowEvent.h>
#include<iostream>

WindowEvent::WindowEvent(HWND hwnd) : m_hwnd(hwnd) {
	//現在のカーソル位置の取得に変更する必要あり
	POINT p;
	GetCursorPos(&p);
	ScreenToClient(hwnd, &p);

	m_CursorState.curr = { p.x,p.y };
	m_CursorState.prev = m_CursorState.curr;

	m_CursorMove = {0.0f,0.0f};
}

WindowEvent::~WindowEvent() {}

void WindowEvent::ComputeCursorMove(/*const CursorState& state*/) {
	float dx = static_cast<float>(m_CursorState.curr.x - m_CursorState.prev.x);
	float dy = static_cast<float>(m_CursorState.curr.y - m_CursorState.prev.y);

	m_CursorMove.x = dx;
	m_CursorMove.y = dy;

	if (dx != 0.0f || dy != 0.0f) {
		m_CursorMove.Normalize();
	}
}

void WindowEvent::UpdateState() {
	
	POINT p;
	GetCursorPos(&p);
	ScreenToClient(m_hwnd, &p);

	/*std:: cout << "prev = (" << m_CursorState.prev.x << ", " << m_CursorState.prev.y << ")"
		<< "curr = (" << m_CursorState.curr.x << ", " << m_CursorState.curr.y << ")" << std::endl << std::endl;*/

	m_CursorState.prev = m_CursorState.curr;
	m_CursorState.curr = { p.x,p.y };
	ComputeCursorMove();

	/*std::cout << "UpdateState" << std::endl << std::endl;

	std::cout << "prev = (" << m_CursorState.prev.x << ", " << m_CursorState.prev.y << ")"
		<< "curr = (" << m_CursorState.curr.x << ", " << m_CursorState.curr.y << ")" << std::endl << std::endl;*/
	//std::cout << "(" << m_CursorMove.x << ", " << m_CursorMove.y << ")" << std::endl << std::endl;
	RECT rc{};
	/*if (!GetClientRect(m_hwnd, &rc)) {
		std::cout << "GetClientRect failed. hwnd = " << m_hwnd << std::endl;
	}
	else {
		std::cout << "Client size = " << rc.right << " x " << rc.bottom << std::endl;
	}*/
}