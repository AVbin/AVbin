!include Library.nsh
!include LogicLib.nsh
!include MUI2.nsh

BrandingText "AVbin  http://avbin.github.org"

Function .onInit
    ${If} $INSTDIR == "" ; /D= was not used on the command line
        ${If} "@ARCH@" == "64"
            StrCpy $INSTDIR $PROGRAMFILES64\AVbin
        ${Else}
            StrCpy $INSTDIR $PROGRAMFILES\AVbin
        ${EndIf}
    ${EndIf}
FunctionEnd

SetCompressor /SOLID lzma

name "AVbin @AVBIN_VERSION@ @ARCH@-bit"

#PageEx license
#    LicenseText "Read Me"
#    LicenseData ../README
#PageExEnd

#Page license

#Page instfiles

# See file:///usr/share/doc/nsis/Docs/Modern%20UI%202/Readme.html

#!define MUI_LICENSEPAGE_BUTTON_TEXT "Continue"
#!insertmacro MUI_PAGE_LICENSE "../README"

!insertmacro MUI_PAGE_LICENSE "../COPYING.LESSER"

!insertmacro MUI_PAGE_INSTFILES

Section "AVbin Library"
    CreateDirectory $INSTDIR
    Var /GLOBAL ALREADY_INSTALLED

    # It appears this definition works for the uninstaller stuff below as well
    ${If} "@ARCH@" == "64"
        !define LIBRARY_X64
    ${ENDIF}

    IfFileExists "$SYSDIR\@AVBIN_LIB_FILENAME@" 0 new_installation
        StrCpy $ALREADY_INSTALLED 1
    new_installation:
   
    !insertmacro InstallLib DLL \
        $ALREADY_INSTALLED \
        NOREBOOT_NOTPROTECTED \
        win@ARCH@/@AVBIN_LIB_FILENAME@ \
        $SYSDIR\@AVBIN_LIB_FILENAME@ \
        $SYSDIR

    # Install an uninstaller into INSTDIR
    WriteUninstaller AVbin@AVBIN_VERSION@-win@ARCH@-uninstaller.exe
    CreateDirectory "$SMPROGRAMS\AVbin"
    CreateShortCut "$SMPROGRAMS\AVbin\Uninstall AVbin.lnk" "$INSTDIR\AVbin@AVBIN_VERSION@-win@ARCH@-uninstaller.exe"
SectionEnd


# Stuff for the WriteUninstaller command
Section "un.AVbin Library"
    !insertmacro UnInstallLib DLL \
        SHARED \
        NOREBOOT_NOTPROTECTED \
        $SYSDIR\@AVBIN_LIB_FILENAME@
    Delete "$INSTDIR\AVbin@AVBIN_VERSION@-win@ARCH@-uninstaller.exe"
    RMDir "$INSTDIR"
    Delete "$SMPROGRAMS\AVbin\Uninstall AVbin.lnk"
    RMDir "$SMPROGRAMS\AVbin"
SectionEnd


OutFile AVbin@AVBIN_VERSION@-win@ARCH@.exe

