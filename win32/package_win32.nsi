!ifndef MINGW_PATH
	!define MINGW_PATH /usr/lib/gcc/i686-w64-mingw32/4.6
!endif
!ifndef PTHREAD_PATH
	!define PTHREAD_PATH /usr/i686-w64-mingw32/lib
!endif
!ifndef OPENSSL_PATH
	!define OPENSSL_PATH /opt/mingw32/mingw32/bin
!endif

# Define the path of the build directory
!ifndef BDIR
	!define BDIR "build.w32"
!endif

SetCompressor /FINAL /SOLID lzma

;-------------------
; Include Modern UI
	!include "MUI2.nsh"

	!define MUI_ICON "../src/gui/rc/nvagent.ico"
	!define MUI_UNICON "../src/gui/rc/nvagent.ico"

	!define MUI_HEADERIMAGE
	!define MUI_HEADERIMAGE_RIGH
	!define MUI_HEADERIMAGE_BITMAP "./graphics/Header/orange-r.bmp"
	!define MUI_HEADERIMAGE_UNBITMAP "./graphics/Header/orange-uninstall-r.bmp"

	!define MUI_WELCOMEFINISHPAGE_BITMAP "./graphics/Wizard/orange.bmp"
	!define MUI_UNWELCOMEFINISHPAGE_BITMAP "./graphics/Wizard/orange-uninstall.bmp"

; --------
; General
	!include "x64.nsh"
	!define /date NOW "%y.%m.%d"
	Name "Netvfy Agent GUI"
	!ifndef OUTFILE
		!define OUTFILE "${BDIR}/netvfy-agent-gui-${NOW}_x86.exe"
	!endif
	OutFile "${OUTFILE}"
	InstallDir $PROGRAMFILES\netvfy-agent-gui

	; Ask admin privileges
	RequestExecutionLevel admin
	ShowInstDetails show
	ShowUninstDetails show

;-------
; Pages
	; Install
	!insertmacro MUI_PAGE_WELCOME
	!insertmacro MUI_PAGE_COMPONENTS
	!insertmacro MUI_PAGE_DIRECTORY

	; Start Menu Folder Page Configuration
	Var StartMenuFolder
	!define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKCU"
	!define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\netvfy-agent"
	!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
	!insertmacro MUI_PAGE_STARTMENU Application $StartMenuFolder

	!insertmacro MUI_PAGE_INSTFILES

	; Uninstall
	!insertmacro MUI_UNPAGE_CONFIRM
	!insertmacro MUI_UNPAGE_INSTFILES

;-----------
; Languages
	!insertmacro MUI_LANGUAGE "English"

;-------------------
; Installer section
	Section "Netvfy Agent GUI"
		setOutPath $INSTDIR

		File ${BDIR}/src/netvfy-agent.exe
		File ${TAPCFG_PATH}/build/tapcfg.dll
		File ${MINGW_PATH}/libgcc_s_sjlj-1.dll
		File ${MINGW_PATH}/libstdc++-6.dll
		File ${MINGW_PATH}/libssp-0.dll
		File /usr/i686-w64-mingw32/lib/libwinpthread-1.dll
		File ${LIBRESSL_PATH}/ssl/.libs/libssl-47.dll
		File ${LIBRESSL_PATH}/crypto/.libs/libcrypto-45.dll
		File ${LIBEVENT_PATH}/.libs/libevent-2-0-5.dll
		File ${LIBEVENT_PATH}/.libs/libevent_core-2-0-5.dll
		File ${LIBEVENT_PATH}/.libs/libevent_extra-2-0-5.dll
		File ${LIBEVENT_PATH}/.libs/libevent_openssl-2-0-5.dll
		File ${LIBJANSSON_PATH}/src/.libs/libjansson-4.dll
		File ${LIBCURL_PATH}/lib/.libs/libcurl-4.dll
		File ${LIBWX_PATH}/lib/gcc810_dll/wxbase312u_gcc810.dll
		File ${LIBWX_PATH}/lib/gcc810_dll/wxmsw312u_core_gcc810.dll
		File /oname=curl-ca-bundle.crt ${LIBCURL_PATH}/lib/ca-bundle.crt

		CreateDirectory $APPDATA\netvfy\default

		; Create uninstaller
		WriteUninstaller "$INSTDIR\netvfy-agent-uninstall.exe"

		!insertmacro MUI_STARTMENU_WRITE_BEGIN Application
			CreateDirectory "$SMPROGRAMS\$StartMenuFolder"
			CreateShortCut  "$DESKTOP\netvfy-agent.lnk" "$INSTDIR\netvfy-agent.exe"
			CreateShortCut  "$SMPROGRAMS\$StartMenuFolder\netvfy-agent.lnk" "$INSTDIR\netvfy-agent.exe"
			CreateShortCut  "$SMPROGRAMS\$StartMenuFolder\netvfy-agent-uninstall.lnk" "$INSTDIR\netvfy-agent-uninstall.exe"
		!insertmacro MUI_STARTMENU_WRITE_END

		; Update icons cache
		System::Call 'Shell32::SHChangeNotify(i 0x8000000, i 0, i 0, i 0)'

	sectionEnd

	Section "TAP Virtual Ethernet Adapter" SecTAP
		SetOverwrite on
		setOutPath "$TEMP\"

		File /r tap-driver-32_64/
		DetailPrint "TAP INSTALL (May need confirmation)"

		${If} ${RunningX64}
			setOutPath "$TEMP\64-bit\"
			nsExec::ExecToLog '"deltapall.bat" /S /SELECT_UTILITIES=1'
			nsExec::ExecToLog '"addtap.bat" /S /SELECT_UTILITIES=1'
		${Else}
			setOutPath "$TEMP\32-bit\"
			nsExec::ExecToLog '"deltapall.bat" /S /SELECT_UTILITIES=1'
			nsExec::ExecToLog '"addtap.bat" /S /SELECT_UTILITIES=1'
		${EndIf}

	sectionEnd

;---------------------
; Uninstaller section
	Section "Uninstall"
		Delete "$INSTDIR\*"
		RMDir "$INSTDIR"

		Delete "$APPDATA\netvfy\*"
		RMDir "$APPDATA\netvfy"

		!insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuFolder
		Delete "$DESKTOP\netvfy-agent.lnk"
		Delete "$SMPROGRAMS\$StartMenuFolder\netvfy-agent.lnk"
		Delete "$SMPROGRAMS\$StartMenuFolder\netvfy-agent-uninstall.lnk"
		RMDir "$SMPROGRAMS\$StartMenuFolder"

		StrCpy $2 $INSTDIR "" 3
		Delete "$LOCALAPPDATA\VirtualStore\$2\*"
		RMDir "$LOCALAPPDATA\VirtualStore\$2"

		DeleteRegKey /ifempty HKCU "Software\netvfy-agent"
	SectionEnd
