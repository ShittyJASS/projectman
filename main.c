#include "application.h"
#include <Windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPSTR lpCmdLine, int nShowCmd) {
	return execute_application(__argc, __argv);
}
