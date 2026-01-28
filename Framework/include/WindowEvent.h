#pragma once 
#include <cmath>
#include <vector>
struct Pos {
	int x;
	int y;
};

struct CursorState {
	Pos prev;
	Pos curr;
};

struct CursorMove {
//³‹K‰»‚³‚ê‚½ƒxƒNƒgƒ‹
	float x;
	float y;

	float Length() const{
		return std::sqrt(x * x + y * y);
	}

	void Normalize() {
		float len = Length();
		if (len < 1e-6f) return;

		x /= len;
		y /= len;
	}
};

struct EulerAngles {
	float          Pitch;   //x²‰ñ“]
	float          Yaw;     //y²‰ñ“]
	float          Roll;    //z²‰ñ“]
};

class WindowEvent {

public:
	//WindowEvent() = default;
	explicit WindowEvent(HWND hwnd);
	~WindowEvent();

	void UpdateState();
	void ComputeCursorMove(/*const CursorState& state*/);

private:
	HWND           m_hwnd;
	CursorState    m_CursorState;
	CursorMove     m_CursorMove;
	EulerAngles    m_EulerAngles;

};