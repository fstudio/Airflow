///
///
#include <string>
#include <Windows.h>
#include <Wingdi.h>
#include <d2d1.h>
#include <dwrite.h>
#include <strsafe.h>

class RemoveFontEvent {
private:
	std::wstring fontFile_;
public:
	void RemoveFontBind(const wchar_t *fontFile)
	{
		fontFile_ = fontFile;
	}
	~RemoveFontEvent()
	{
		if (!fontFile_.empty()) {
			RemoveFontResourceExW(fontFile_.c_str(), FR_PRIVATE, 0);
			//::SendMessage(HWND_BROADCAST, WM_FONTCHANGE, 0, 0);
		}
	}
};

static RemoveFontEvent removeFontEvent;


BOOL WINAPI InitializeAwesomeFont(LPCWSTR fontFile)
{
	if (!fontFile)
		return FALSE;
	if (!AddFontResourceExW(fontFile, FR_PRIVATE, 0)) {
		MessageBoxW(nullptr, L"Cannot Add font to resource", fontFile, MB_OK);
		return FALSE;
	}
	removeFontEvent.RemoveFontBind(fontFile);
	return true;
}