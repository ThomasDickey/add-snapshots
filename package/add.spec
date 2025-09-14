Summary: Full-screen editing calculator
# $Id: add.spec,v 1.27 2025/09/14 09:50:39 tom Exp $
Name: add
Version: 20250914
Release: 1
License: MIT
Group: Applications/Development
URL: https://invisible-island.net/%{AppProgram}/
Source0: %{name}-%{version}.tgz
Packager: Thomas Dickey <dickey@invisible-island.net>

%description
Add  performs  fixed-point  computation.   It  is designed for use as a
checkbook or expense-account balancing tool.

Add maintains a running result for each operation.  You may  scroll  to
any position in the expression list and modify the list.  Enter data by
typing numbers (with optional decimal point), separated by operators.

An output transcript may be saved and  reloaded  for  further  editing.
Scripts  are  loaded  from  left  to right (with the "output" processed
first).

%prep

# no need for debugging symbols...
%define debug_package %{nil}

%setup -q -n %{name}-%{version}

%build

INSTALL_PROGRAM='${INSTALL}' \
%configure \
 --target %{_target_platform} \
 --prefix=%{_prefix} \
 --bindir=%{_bindir} \
 --libdir=%{_libdir} \
 --datadir=%{_datarootdir}/%{name} \
 --mandir=%{_mandir} \
 --enable-warnings \
 --enable-stdnoreturn \
 --with-man2html

make

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

make install                    DESTDIR=$RPM_BUILD_ROOT

strip $RPM_BUILD_ROOT%{_bindir}/%{name}

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_bindir}/%{name}
%{_datarootdir}/%{name}/%{name}.hlp
%{_mandir}/man1/%{name}.*

%changelog
# each patch should add its ChangeLog entries here

* Sat Sep 13 2025 Thomas E. Dickey
- testing add 20250914-1

* Wed Dec 22 2021 Thomas Dickey
- move ".hlp" file to subdirectory, omit "x+" from package

* Sun Apr 01 2018 Thomas Dickey
- update ftp url, add "x+" to package, suppress debug-symbols

* Wed Jul 07 2010 Thomas Dickey
- initial version
