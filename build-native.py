# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 LIghtJUNction
from pathlib import Path
import subprocess,concurrent.futures,os,argparse
p=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description="Build opt-in Zygisk libraries and Android namespace self-test")
parser.add_argument('--ndk',type=Path,help='Android NDK installation directory')
args=parser.parse_args()
ndk_home=args.ndk
if ndk_home is None:
    configured=os.environ.get('ANDROID_NDK_HOME') or os.environ.get('ANDROID_NDK_ROOT')
    if configured: ndk_home=Path(configured)
if ndk_home is None:
    sdk=Path(os.environ.get('ANDROID_HOME') or os.environ.get('ANDROID_SDK_ROOT') or '/opt/android-sdk')
    versions=sorted((sdk/'ndk').glob('*'),key=lambda q:tuple(int(x) for x in q.name.split('.') if x.isdigit()))
    if versions: ndk_home=versions[-1]
if ndk_home is None: parser.error('Pass --ndk or set ANDROID_NDK_HOME')
hosts=list((ndk_home/'toolchains/llvm/prebuilt').glob('*'))
if len(hosts)!=1: parser.error('Expected one host toolchain in NDK prebuilt directory')
ndk=hosts[0]/'bin'
common=['-std=c++20','-O2','-fno-exceptions','-fno-rtti','-fvisibility=hidden','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-Wl,--exclude-libs,ALL','-Wl,-z,max-page-size=16384','-static-libstdc++']
def build(abi,triple):
 out=p/'src/art_fd_rebind/zygisk'/f'{abi}.so'
 cmd=[str(ndk/(triple+'26-clang++')),*common,'-shared','-fPIC',str(p/'native/module.cpp'),str(p/'native/fdutils/fd_utils.cpp'),'-llog','-o',str(out)]
 subprocess.run(cmd,check=True);subprocess.run([str(ndk/'llvm-strip'),'--strip-unneeded',str(out)],check=True)
 print(abi,out.stat().st_size)
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as ex:
 for f in [ex.submit(build,'arm64-v8a','aarch64-linux-android'),ex.submit(build,'armeabi-v7a','armv7a-linux-androideabi')]:f.result()
(p/'build').mkdir(exist_ok=True)
subprocess.run([str(ndk/'aarch64-linux-android26-clang++'),*common,str(p/'native/selftest.cpp'),str(p/'native/fdutils/fd_utils.cpp'),'-llog','-o',str(p/'build/art-fd-selftest')],check=True)
