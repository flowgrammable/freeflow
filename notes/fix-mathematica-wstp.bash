#!/bin/bash

otool -l /Applications/Mathematica.app/Contents/SystemFiles/Links/WSTP/DeveloperKit/MacOSX-x86-64/CompilerAdditions/wstp.framework/Versions/4.36/wstp

install_name_tool -id '/Applications/Mathematica.app/Contents/SystemFiles/Links/WSTP/DeveloperKit/MacOSX-x86-64/CompilerAdditions/wstp.framework/Versions/4.36/wstp' /Applications/Mathematica.app/Contents/SystemFiles/Links/WSTP/DeveloperKit/MacOSX-x86-64/CompilerAdditions/wstp.framework/Versions/4.36/wstp
