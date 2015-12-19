/*********************************************************************************************************
* Airflow.cc
* Note: Phoenix Airflow tools
* Date: @2015.05
* Author: Force Charlie
* E-mail:<forcemz@outlook.com>
* Copyright (C) 2015 The ForceStudio All Rights Reserved.
**********************************************************************************************************/
#include "Airflow.h"
#include "resource.h"
#include <Prsht.h>
#include <CommCtrl.h>
#include <iostream>
#include <Shlwapi.h>
#include <atlbase.h>
#include <atlwin.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <string>

#pragma comment(lib,"d2d1.lib")
#pragma comment(lib,"windowscodecs.lib")
#pragma comment(lib,"dwrite.lib")


#ifndef HINST_THISCOMPONENT
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#endif

template<class Interface>
inline void
SafeRelease(
Interface **ppInterfaceToRelease
)
{
	if (*ppInterfaceToRelease != NULL) {
		(*ppInterfaceToRelease)->Release();

		(*ppInterfaceToRelease) = NULL;
	}
}

#define SAFE_RELEASE(P) if(P){P->Release() ; P = NULL ;}

static const wchar_t * stdioimage()
{
	static WCHAR szTemp[MAX_PATH] = { 0 };
	GetTempPathW(MAX_PATH, szTemp);
	wcscat_s(szTemp, MAX_PATH, L"Airflow.Standrand.IO.API.v1.log");
	return szTemp;
}

class RedirectStdIO {
private:
	bool isOpen;
public:
	RedirectStdIO()
	{
		FILE *stream;
		auto err = _wfreopen_s(&stream, stdioimage(), L"w+t", stdout);
		err = _wfreopen_s(&stream, stdioimage(), L"w+", stderr);
		if (err == 0)
			isOpen = true;
	}
	~RedirectStdIO()
	{
		////
		fflush(stdout);
		fclose(stdout);
		fclose(stderr);
	}
};

const LPCWSTR PackageSubffix[] = { L".msu", L".msp", L".msi", L".cab" };
std::vector<std::wstring> vFileList;
#define MAXPAGES 5

HRESULT LoadResourceBitmap(
	ID2D1RenderTarget *pRenderTarget,
	IWICImagingFactory *pIWICFactory,
	PCWSTR resourceName,
	PCWSTR resourceType,
	UINT destinationWidth,
	UINT destinationHeight,
	ID2D1Bitmap **ppBitmap
	);

class AirflowWindow
	: public CWindowImpl<AirflowWindow, CWindow, CWinTraits<WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS&~WS_MAXIMIZEBOX, 0> > {
public:
	RECT mRect;
	DECLARE_WND_CLASS(_T("Airflow.UI.Render.Window"))
	BEGIN_MSG_MAP(AirflowWindow)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_DISPLAYCHANGE, OnDisplayChange)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_DROPFILES, OnDropfiles)
		MESSAGE_HANDLER(WM_ASYNCHRONOUS_NOTIFY_MSG, OnAsynNotify)
		COMMAND_ID_HANDLER(IDC_BUTTON_OPENFILE, OnOpenFile)
		COMMAND_ID_HANDLER(IDC_BUTTON_OPENDIR, OnOpenDIR)
		COMMAND_ID_HANDLER(IDC_BUTTON_ENTER, OnOptionsEnter)
		COMMAND_ID_HANDLER(IDC_BUTTON_CANCEL, OnOptionsCancel)
	END_MSG_MAP()
	LRESULT OnCreate(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL & bHandled)
	{
		auto icon = LoadIconW(HINST_THISCOMPONENT, MAKEINTRESOURCEW(IDI_WIZICON));
		SetIcon(icon, TRUE);
		ChangeWindowMessageFilter(WM_DROPFILES, MSGFLT_ADD);
		ChangeWindowMessageFilter(WM_COPYDATA, MSGFLT_ADD);
		ChangeWindowMessageFilter(0x0049, MSGFLT_ADD);
		::DragAcceptFiles(m_hWnd, TRUE);
		InitCommonControls();
		HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
		LOGFONT uifont = { 0 };
		GetObject(hFont, sizeof(uifont), &uifont);
		DeleteObject(hFont);
		hFont = NULL;
		uifont.lfHeight = 20;
		uifont.lfWeight = FW_NORMAL;
		wcscpy_s(uifont.lfFaceName, L"Segoe UI");
		hFont = CreateFontIndirectW(&uifont);
		DWORD dwButton = WS_CHILDWINDOW | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | BS_TEXT;
		DWORD dwButtonEx = WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR | WS_EX_NOPARENTNOTIFY;
		DWORD dwEdit = WS_CHILDWINDOW | WS_CLIPSIBLINGS | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL;;
		DWORD dwEditEx = WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR | WS_EX_NOPARENTNOTIFY | WS_EX_CLIENTEDGE;
		DWORD dwProgressEx = WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR | WS_EX_NOPARENTNOTIFY;
		DWORD dwProgress = WS_CHILDWINDOW | WS_CLIPSIBLINGS | WS_VISIBLE;

		HWND hEditURL = CreateWindowExW(dwEditEx, WC_EDIT, L"", dwEdit, 150, 90, 380, 24, m_hWnd,
										HMENU(IDC_EDIT_FILEURL), HINST_THISCOMPONENT, nullptr);
		HWND hEditFolder = CreateWindowExW(dwEditEx, WC_EDIT, L"", dwEdit, 150, 140, 380, 24, m_hWnd,
										   HMENU(IDC_EDIT_FOLDER), HINST_THISCOMPONENT, nullptr);
		HWND hBURL = CreateWindowExW(dwButtonEx, WC_BUTTON, L"View...", dwButton, 550, 90, 100, 24, m_hWnd,
									 HMENU(IDC_BUTTON_OPENFILE), HINST_THISCOMPONENT, nullptr);
		HWND hbFolder = CreateWindowExW(dwButtonEx, WC_BUTTON, L"New Folder...", dwButton, 550, 140, 100, 24, m_hWnd,
										HMENU(IDC_BUTTON_OPENDIR), HINST_THISCOMPONENT, nullptr);
		HWND hProgress = CreateWindowExW(dwProgressEx, PROGRESS_CLASS, L"", dwProgress, 150, 200, 380, 22, m_hWnd,
										 HMENU(IDC_PROCESS_RATE), HINST_THISCOMPONENT, nullptr);
		//::UpdateWindow(hEditURL);
		HWND hBEnter = CreateWindowExW(dwButtonEx, WC_BUTTON, L"Enter", dwButton, 210, 250, 120, 25, m_hWnd,
									   HMENU(IDC_BUTTON_ENTER), HINST_THISCOMPONENT, nullptr);
		HWND hBCancle = CreateWindowExW(dwButtonEx, WC_BUTTON, L"Cancel", dwButton, 410, 250, 120, 25, m_hWnd,
										HMENU(IDC_BUTTON_CANCEL), HINST_THISCOMPONENT, nullptr);
		SendMessage(hEditURL, WM_SETFONT, (WPARAM)hFont, lParam);
		SendMessage(hEditFolder, WM_SETFONT, (WPARAM)hFont, lParam);
		SendMessage(hBURL, WM_SETFONT, (WPARAM)hFont, lParam);
		SendMessage(hbFolder, WM_SETFONT, (WPARAM)hFont, lParam);
		SendMessage(hBEnter, WM_SETFONT, (WPARAM)hFont, lParam);
		SendMessage(hBCancle, WM_SETFONT, (WPARAM)hFont, lParam);
		return S_OK;
	}
	LRESULT OnPaint(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL & bHandled)
	{
		OnRender();
		ValidateRect(NULL);
		return 0;
	}
	LRESULT OnSize(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL & bHandled)
	{
		UINT width = LOWORD(lParam);
		UINT height = HIWORD(lParam);
		this->OnResize(width, height);
		return S_OK;
	}
	LRESULT OnDisplayChange(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL & bHandled)
	{
		::InvalidateRect(m_hWnd, NULL, FALSE);
		//::InvalidateRect(GetDlgItem(IDC_EDIT_FILEURL), NULL, FALSE);
		return S_OK;
	}
	LRESULT OnDestroy(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL & bHandled)
	{
		///Kill Woker Threads
		PostQuitMessage(0);
		return S_OK;
	}
	LRESULT OnDropfiles(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL & bHandled)
	{
		HDROP hDrop = (HDROP)wParam;
		UINT nFileNum = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
		WCHAR strFileName[MAX_PATH];
		for (UINT i = 0; i < nFileNum; i++) {
			DragQueryFileW(hDrop, i, strFileName, MAX_PATH);
			if (PathFindSuffixArrayW(strFileName, PackageSubffix, ARRAYSIZE(PackageSubffix))) {
				vFileList.push_back(strFileName);
			}
			std::cout << vFileList.size() << std::endl;
			if (!vFileList.empty()) {
				::SetWindowTextW(::GetDlgItem(m_hWnd, IDC_EDIT_FILEURL), vFileList[0].c_str());
			}
		}
		DragFinish(hDrop);
		::InvalidateRect(m_hWnd, NULL, TRUE);
		return S_OK;
	}
	LRESULT OnAsynNotify(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL & bHandled)
	{
		::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_ENTER), TRUE);
		::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_OPENDIR), TRUE);
		::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_OPENFILE), TRUE);
		::EnableWindow(::GetDlgItem(m_hWnd, IDC_EDIT_FOLDER), TRUE);
		::EnableWindow(::GetDlgItem(m_hWnd, IDC_EDIT_FILEURL), TRUE);
		return S_OK;
	}
	///Command
	LRESULT OnOpenFile(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		std::wstring file;
		auto ret = AirflowFileOpenWindow(m_hWnd, file, L"Open Installer and Update Package");
		if (ret) {
			::SetWindowTextW(hWndCtl, file.c_str());
			////toCheck file type
			std::wcout << file << std::endl;
		}
		return S_OK;
	}
	LRESULT OnOpenDIR(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		std::wstring folder;
		auto ret = AirflowFolderOpenWindow(m_hWnd, folder, L"Open Installer and Update Package");
		if (ret) {
			::SetWindowTextW(hWndCtl, folder.c_str());
			////toCheck file type
		}
		return S_OK;
	}
	LRESULT OnOptionsEnter(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandle)
	{
		WCHAR szPackagePath[4096] = { 0 };
		WCHAR szRecover[4096] = { 0 };
		::GetWindowTextW(::GetDlgItem(m_hWnd, IDC_EDIT_FILEURL), szPackagePath, 4096);
		::GetWindowText(::GetDlgItem(m_hWnd, IDC_EDIT_FOLDER), szRecover, 4096);
		if (CheckPackageAfterLayout(szPackagePath, 4096, szRecover, 4096)) {
			AirflowTaskData *data = new AirflowTaskData();
			data->isForce = false;
			data->sendRate = true;
			data->uMsgid = WM_ASYNCHRONOUS_NOTIFY_MSG;
			data->hWnd = m_hWnd;
			data->mRate = IDC_PROCESS_RATE;
			data->rawfile = szPackagePath;
			data->outdir = szRecover;
			DWORD tId;
			HANDLE hThread = CreateThread(NULL, 0, BackgroundWorker, data, 0, &tId);
			if (!hThread) {
				::MessageBoxW(m_hWnd, L"CreateThread Failed", L"Error", MB_OK);
			} else {
				::SendDlgItemMessage(m_hWnd, IDC_PROCESS_RATE, PBM_SETPOS, 50, 0L);
				::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_ENTER), FALSE);
				::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_OPENDIR), FALSE);
				::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_OPENFILE), FALSE);
				::EnableWindow(::GetDlgItem(m_hWnd, IDC_EDIT_FOLDER), FALSE);
				::EnableWindow(::GetDlgItem(m_hWnd, IDC_EDIT_FILEURL), FALSE);
			}
		} else {
			::MessageBoxW(m_hWnd, stdioimage(), L"You can exit and open this log", MB_OK | MB_ICONERROR);
		}
		return S_OK;
	}
	LRESULT OnOptionsCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandle)
	{
		::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_ENTER), TRUE);
		::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_OPENDIR), TRUE);
		::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_OPENFILE), TRUE);
		::EnableWindow(::GetDlgItem(m_hWnd, IDC_EDIT_FOLDER), TRUE);
		::EnableWindow(::GetDlgItem(m_hWnd, IDC_EDIT_FILEURL), TRUE);
		///Kill Thread and Delete Resource
		::SendDlgItemMessage(m_hWnd, IDC_PROCESS_RATE, PBM_SETPOS, 0, 0L);
		return S_OK;
	}
	/////////
	LRESULT Initialize()
	{
		// Initialize device-indpendent resources, such
		// as the Direct2D factory.
		auto hr = CreateDeviceIndependentResources();
		FLOAT dpiX, dpiY;
		m_pD2DFactory->GetDesktopDpi(&dpiX, &dpiY);
		mRect.left = CW_USEDEFAULT;
		mRect.top = CW_USEDEFAULT;
		mRect.right = mRect.left + static_cast<UINT>(ceil(720.f * dpiX / 96.f));
		mRect.bottom = mRect.top + static_cast<UINT>(ceil(400.f * dpiY / 96.f));
		return hr;
	}
	void OnFinalMessage(HWND hwnd)
	{
		::PostQuitMessage(0);
	}
	AirflowWindow::AirflowWindow(AirflowStructure &ars) :
		mRect({ 0 }),
		airFlow(ars),
		m_pD2DFactory(NULL),
		m_pSolidBrush(NULL),
		m_pWICFactory(NULL),
		m_pDWriteFactory(NULL),
		m_pRenderTarget(NULL),
		m_pTextFormat(NULL),
		m_pBitmap(NULL)
	{
	}

	//
	// Release resources.
	//
	AirflowWindow::~AirflowWindow()
	{
		SafeRelease(&m_pD2DFactory);
		SafeRelease(&m_pDWriteFactory);
		SafeRelease(&m_pSolidBrush);
		SafeRelease(&m_pRenderTarget);
		SafeRelease(&m_pTextFormat);
		SafeRelease(&m_pBitmap);
	}
private:
	AirflowStructure airFlow;
	HRESULT CreateDeviceIndependentResources()
	{
		static const WCHAR msc_fontName[] = L"Segoe UI";
		static const FLOAT msc_fontSize = 16;
		HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
		if (SUCCEEDED(hr)) {

			// Create a DirectWrite factory.
			hr = DWriteCreateFactory(
				DWRITE_FACTORY_TYPE_SHARED,
				__uuidof(m_pDWriteFactory),
				reinterpret_cast<IUnknown **>(&m_pDWriteFactory)
				);
		}
		if (SUCCEEDED(hr)) {
			// Create a DirectWrite text format object.
			hr = m_pDWriteFactory->CreateTextFormat(
				msc_fontName,
				NULL,
				DWRITE_FONT_WEIGHT_NORMAL,
				DWRITE_FONT_STYLE_NORMAL,
				DWRITE_FONT_STRETCH_NORMAL,
				msc_fontSize,
				L"", //locale
				&m_pTextFormat
				);
		}
		if (SUCCEEDED(hr)) {
			// Center the text horizontally and vertically.
			m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

			m_pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

		}

		return hr;
	}
	HRESULT CreateDeviceResources()
	{
		HRESULT hr = S_OK;

		if (!m_pRenderTarget) {
			RECT rc;
			::GetClientRect(m_hWnd, &rc);

			D2D1_SIZE_U size = D2D1::SizeU(
				rc.right - rc.left,
				rc.bottom - rc.top
				);

			// Create a Direct2D render target.
			hr = m_pD2DFactory->CreateHwndRenderTarget(
				D2D1::RenderTargetProperties(),
				D2D1::HwndRenderTargetProperties(m_hWnd, size),
				&m_pRenderTarget
				);
			if (SUCCEEDED(hr)) {
				hr = CoCreateInstance(
					CLSID_WICImagingFactory,
					NULL,
					CLSCTX_INPROC_SERVER,
					IID_PPV_ARGS(&m_pWICFactory)
					);
			}
			if (SUCCEEDED(hr)) {
				hr = m_pRenderTarget->CreateSolidColorBrush(
					D2D1::ColorF(D2D1::ColorF::Black),
					&m_pSolidBrush
					);
			}
			// Create a bitmap from an application resource.
			hr = LoadResourceBitmap(
				m_pRenderTarget,
				m_pWICFactory,
				MAKEINTRESOURCE(IDP_BACKGROUND_PNG),
				L"PNG",
				700,
				0,
				&m_pBitmap
				);
		}

		return hr;
	}
	void DiscardDeviceResources()
	{
		SafeRelease(&m_pRenderTarget);
		SafeRelease(&m_pBitmap);
	}

	HRESULT OnRender();

	void OnResize(
		UINT width,
		UINT height
		)
	{
		if (m_pRenderTarget) {
			D2D1_SIZE_U size;
			size.width = width;
			size.height = height;
			m_pRenderTarget->Resize(size);
		}
	}
private:
	ID2D1Factory *m_pD2DFactory;
	ID2D1HwndRenderTarget *m_pRenderTarget;
	ID2D1SolidColorBrush *m_pSolidBrush;
	ID2D1Bitmap *m_pBitmap;
	IWICImagingFactory *m_pWICFactory;
	IDWriteFactory *m_pDWriteFactory;
	IDWriteTextFormat *m_pTextFormat;
};

//////
HRESULT AirflowWindow::OnRender()
{

	HRESULT hr = CreateDeviceResources();

	if (SUCCEEDED(hr)) {
		// Retrieve the size of the render target.
		D2D1_SIZE_F renderTargetSize = m_pRenderTarget->GetSize();

		m_pRenderTarget->BeginDraw();

		m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

		m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));


		// Draw a bitmap in the upper-left corner of the window.
		m_pRenderTarget->DrawBitmap(
			m_pBitmap,
			D2D1::RectF(0.0f, 0.0f, renderTargetSize.width, renderTargetSize.height)
			);
#define WcLength(wstr) (sizeof(wstr)/sizeof(wchar_t)-1)
		const wchar_t pkg[] = L"Package:";
		const wchar_t folder[] = L"Recover Folder: ";
		const wchar_t rate[] = L"Progress Rate: ";
		const wchar_t message[] = L"\x263B \x2665 Airflow";
		m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
		m_pRenderTarget->DrawTextW(pkg, WcLength(pkg), m_pTextFormat,
								   D2D1::RectF(15.0f, 80.0f, 130.0f, 125.0f), m_pSolidBrush,
								   D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_NATURAL);
		m_pRenderTarget->DrawTextW(folder, WcLength(folder), m_pTextFormat,
								   D2D1::RectF(15.0f, 140.0f, 130.0f, 160.0f), m_pSolidBrush,
								   D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_NATURAL);
		m_pRenderTarget->DrawTextW(rate, WcLength(rate), m_pTextFormat,
								   D2D1::RectF(15.0f, 200.0f, 130.0f, 225.0f), m_pSolidBrush,
								   D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_NATURAL);
		m_pRenderTarget->DrawTextW(message, WcLength(message), m_pTextFormat,
								   D2D1::RectF(20.0f, 320.0f, 600.0f, 340.0f), m_pSolidBrush,
								   D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_NATURAL);
		hr = m_pRenderTarget->EndDraw();

		if (hr == D2DERR_RECREATE_TARGET) {
			hr = S_OK;
			DiscardDeviceResources();
		}
	}

	return hr;
}


HRESULT LoadResourceBitmap(
	ID2D1RenderTarget *pRenderTarget,
	IWICImagingFactory *pIWICFactory,
	PCWSTR resourceName,
	PCWSTR resourceType,
	UINT destinationWidth,
	UINT destinationHeight,
	ID2D1Bitmap **ppBitmap
	)
{
	HRESULT hr = S_OK;

	IWICBitmapDecoder* pDecoder = NULL;
	IWICBitmapFrameDecode* pSource = NULL;
	IWICStream* pStream = NULL;
	IWICFormatConverter* pConverter = NULL;
	IWICBitmapScaler* pScaler = NULL;

	HRSRC imageResHandle = NULL;
	HGLOBAL imageResDataHandle = NULL;
	void* pImageFile = NULL;
	DWORD imageFileSize = 0;

	// Find the resource then load it
	imageResHandle = FindResource(HINST_THISCOMPONENT, resourceName, resourceType);
	hr = imageResHandle ? S_OK : E_FAIL;
	if (SUCCEEDED(hr)) {
		imageResDataHandle = LoadResource(HINST_THISCOMPONENT, imageResHandle);

		hr = imageResDataHandle ? S_OK : E_FAIL;
	}

	// Lock the resource and calculate the image's size
	if (SUCCEEDED(hr)) {
		// Lock it to get the system memory pointer
		pImageFile = LockResource(imageResDataHandle);

		hr = pImageFile ? S_OK : E_FAIL;
	}
	if (SUCCEEDED(hr)) {
		// Calculate the size
		imageFileSize = SizeofResource(HINST_THISCOMPONENT, imageResHandle);

		hr = imageFileSize ? S_OK : E_FAIL;
	}

	// Create an IWICStream object
	if (SUCCEEDED(hr)) {
		// Create a WIC stream to map onto the memory
		hr = pIWICFactory->CreateStream(&pStream);
	}

	if (SUCCEEDED(hr)) {
		// Initialize the stream with the memory pointer and size
		hr = pStream->InitializeFromMemory(
			reinterpret_cast<BYTE*>(pImageFile),
			imageFileSize
			);
	}

	// Create IWICBitmapDecoder
	if (SUCCEEDED(hr)) {
		// Create a decoder for the stream
		hr = pIWICFactory->CreateDecoderFromStream(
			pStream,
			NULL,
			WICDecodeMetadataCacheOnLoad,
			&pDecoder
			);
	}

	// Retrieve a frame from the image and store it in an IWICBitmapFrameDecode object
	if (SUCCEEDED(hr)) {
		// Create the initial frame
		hr = pDecoder->GetFrame(0, &pSource);
	}

	// Before Direct2D can use the image, it must be converted to the 32bppPBGRA pixel format.
	// To convert the image format, use the IWICImagingFactory::CreateFormatConverter method to create an IWICFormatConverter object, then use the IWICFormatConverter object's Initialize method to perform the conversion.
	if (SUCCEEDED(hr)) {
		// Convert the image format to 32bppPBGRA
		// (DXGI_FORMAT_B8G8R8A8_UNORM + D2D1_ALPHA_MODE_PREMULTIPLIED).
		hr = pIWICFactory->CreateFormatConverter(&pConverter);
	}

	if (SUCCEEDED(hr)) {
		// If a new width or height was specified, create and
		// IWICBitmapScaler and use it to resize the image.
		if (destinationWidth != 0 || destinationHeight != 0) {
			UINT originalWidth;
			UINT originalHeight;
			hr = pSource->GetSize(&originalWidth, &originalHeight);
			if (SUCCEEDED(hr)) {
				if (destinationWidth == 0) {
					FLOAT scalar = static_cast<FLOAT>(destinationHeight) / static_cast<FLOAT>(originalHeight);
					destinationWidth = static_cast<UINT>(scalar * static_cast<FLOAT>(originalWidth));
				} else if (destinationHeight == 0) {
					FLOAT scalar = static_cast<FLOAT>(destinationWidth) / static_cast<FLOAT>(originalWidth);
					destinationHeight = static_cast<UINT>(scalar * static_cast<FLOAT>(originalHeight));
				}

				hr = pIWICFactory->CreateBitmapScaler(&pScaler);
				if (SUCCEEDED(hr)) {
					hr = pScaler->Initialize(
						pSource,
						destinationWidth,
						destinationHeight,
						WICBitmapInterpolationModeCubic
						);
					if (SUCCEEDED(hr)) {
						hr = pConverter->Initialize(
							pScaler,
							GUID_WICPixelFormat32bppPBGRA,
							WICBitmapDitherTypeNone,
							NULL,
							0.f,
							WICBitmapPaletteTypeMedianCut
							);
					}
				}
			}
		}

		else // use default width and height
		{
			hr = pConverter->Initialize(
				pSource,
				GUID_WICPixelFormat32bppPBGRA,
				WICBitmapDitherTypeNone,
				NULL,
				0.f,
				WICBitmapPaletteTypeMedianCut
				);
		}
	}

	// Finally, Create an ID2D1Bitmap object, that can be drawn by a render target and used with other Direct2D objects
	if (SUCCEEDED(hr)) {
		// Create a Direct2D bitmap from the WIC bitmap
		hr = pRenderTarget->CreateBitmapFromWicBitmap(
			pConverter,
			NULL,
			ppBitmap
			);
	}

	SAFE_RELEASE(pDecoder);
	SAFE_RELEASE(pSource);
	SAFE_RELEASE(pStream);
	SAFE_RELEASE(pConverter);
	SAFE_RELEASE(pScaler);

	return hr;
}

int AirflowUIChannel(AirflowStructure &cArgs)
{
	//wchar_t szPath[4096];
	//GetModuleFileNameW(nullptr, szPath, 4096);
	//PathRemoveFileSpecW(szPath);
	//wcscat_s(szPath, L"\\FontAwesome.otf");
	//InitializeAwesomeFont(szPath);
	AirflowWindow airflowUI(cArgs);
	airflowUI.Initialize();
	airflowUI.Create(NULL, airflowUI.mRect, _T("Airflow -Recover Microsoft Installer and Update File"));
	airflowUI.ShowWindow(SW_SHOWNORMAL);
	airflowUI.UpdateWindow();
	MSG msg;
	msg.message = ~(UINT)WM_QUIT;
	while (msg.message != WM_QUIT) {
		if (::GetMessage(&msg, NULL, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return (int)msg.wParam;
}