Name: linux-mount-auditor
Version: 1.0.0
Release: 1%{?dist}
Summary: Read-only Linux mount configuration auditor
License: MIT
URL: https://github.com/guhshosting/linux-mount-auditor
Source0: %{name}-%{version}-src.tar.gz
BuildRequires: cmake gcc-c++

%description
Inspect mountinfo and fstab without modifying system state.

%prep
%setup -q -n %{name}-%{version}
%build
%cmake
%cmake_build
%install
%cmake_install
%check
%ctest
%files
%license LICENSE
%doc README.md
%{_bindir}/linux-mount-auditor
%{_mandir}/man1/linux-mount-auditor.1*
%changelog
* Thu Sep 17 2026 GuhsHosting <guhshosting@users.noreply.github.com> - 1.0.0-1
- Initial release
