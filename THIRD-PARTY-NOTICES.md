# Third-party components

ClipStamp dynamically uses Qt 6.8.3 (Qt Base, Qt Multimedia and runtime dependencies). Qt's open-source license terms vary by component; the LGPLv3/GPLv3 texts are included under licenses. Qt is not modified by this project. Qt sources and exact version: https://download.qt.io/archive/qt/6.8/6.8.3/ and https://code.qt.io/.

FFmpeg is executed as a separate program. The local package may use FFmpeg and dependent DLLs from the installed Shotcut distribution. That build enables GPL components including libx264; it is not an LGPL-only build. Its COPYING file is copied into tools. Source/build references: https://github.com/mltframework/shotcut and https://github.com/mltframework/shotcut/tree/master/scripts; FFmpeg: https://ffmpeg.org/ and https://github.com/FFmpeg/FFmpeg; x264: https://www.videolan.org/developers/x264.html.

The package's tools/BUILD-INFO.txt records the actual FFmpeg version/configuration and included DLLs. Preserve dependency license notices. Before distributing a build to third parties, provide the corresponding source and notices required by the exact Qt/FFmpeg/dependency builds being shipped; repository links alone are not a substitute for fulfilling source-distribution obligations. The packaged application contains no mechanism preventing users from replacing the dynamic libraries or FFmpeg tools.

Noto Sans Thai Regular is bundled unmodified under the SIL Open Font License 1.1. Copyright and license text are included in licenses/NotoSansThai-OFL.txt. Source: https://github.com/notofonts/noto-fonts/tree/main/hinted/ttf/NotoSansThai.

The Windows installer uses NSIS 3.12. Its license notices are included in licenses/NSIS-COPYING.txt. Source: https://nsis.sourceforge.io/ and https://sourceforge.net/projects/nsis/files/NSIS%203/3.12/.
