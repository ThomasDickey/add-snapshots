Summary: add - full-screen editing calculator
%define AppProgram add
%define AppVersion 20200922
# $XTermId: add.spec,v 1.13 2020/09/22 07:49:29 tom Exp $
Name: %{AppProgram}
Version: %{AppVersion}
Release: 1
License: MIT
Group: Applications/Development
URL: ftp://ftp.invisible-island.net/%{AppProgram}
Source0: %{AppProgram}-%{AppVersion}.tgz
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

%setup -q -n %{AppProgram}-%{AppVersion}

%build

INSTALL_PROGRAM='${INSTALL}' \
%configure \
 --target %{_target_platform} \
 --prefix=%{_prefix} \
 --bindir=%{_bindir} \
 --libdir=%{_libdir} \
 --datadir=%{_datadir} \
 --mandir=%{_mandir} \
 --with-man2html

make

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

make install                    DESTDIR=$RPM_BUILD_ROOT

strip $RPM_BUILD_ROOT%{_bindir}/%{AppProgram}

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_bindir}/x+
%{_bindir}/%{AppProgram}
%{_datadir}/%{AppProgram}.hlp
%{_mandir}/man1/%{AppProgram}.*

%changelog
# each patch should add its ChangeLog entries here

* Sun Apr 01 2018 Thomas Dickey
- update ftp url, add "x+" to package, suppress debug-symbols

* Wed Jul 07 2010 Thomas Dickey
- initial version
