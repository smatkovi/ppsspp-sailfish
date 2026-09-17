Name:       ppsspp
Summary:    PPSSPP PSP Emulator for Sailfish OS
Version:    1.19.0
Release:    5
Group:      Applications/Games
License:    GPLv2+
URL:        https://www.ppsspp.org/
Requires:   SDL2, libGLESv2, libEGL

%description
A fast and portable PSP emulator optimized for mobile platforms.

%prep
# Keine Vorbereitung nötig

%build
# Bereits im SB2-Kontext gebaut

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/bin
mkdir -p %{buildroot}/usr/share/ppsspp
mkdir -p %{buildroot}/usr/share/applications

cp -a /home/mersdk/ppsspp/pkg/usr/bin/ppsspp /home/mersdk/ppsspp/pkg/usr/bin/ppsspp-bin %{buildroot}/usr/bin/
cp -a /home/mersdk/ppsspp/pkg/usr/share/ppsspp/assets %{buildroot}/usr/share/ppsspp/
cp -a /home/mersdk/ppsspp/pkg/usr/share/applications/ppsspp.desktop %{buildroot}/usr/share/applications/

%files
%defattr(-,root,root,-)
/usr/bin/ppsspp
/usr/bin/ppsspp-bin
/usr/share/ppsspp/
/usr/share/applications/ppsspp.desktop
