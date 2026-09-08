param(
    [Parameter(Mandatory = $true)]
    [string]$PluginPath
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $PluginPath -PathType Leaf)) {
    throw "Plugin not found: $PluginPath"
}

if ([Environment]::Is64BitProcess) {
    throw 'Run this script with the 32-bit Windows PowerShell from SysWOW64.'
}

$source = @"
using System;
using System.Runtime.InteropServices;

public static class NativePluginLoadTest {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr LoadLibraryEx(
        string fileName, IntPtr file, uint flags);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr GetProcAddress(
        IntPtr module, string name);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate IntPtr Factory();
}
"@
Add-Type -TypeDefinition $source

$fullPath = (Resolve-Path -LiteralPath $PluginPath).Path
$flags = 0x00000100 -bor 0x00001000 # DLL_LOAD_DIR | DEFAULT_DIRS
$module = [NativePluginLoadTest]::LoadLibraryEx(
    $fullPath, [IntPtr]::Zero, [uint32]$flags)
$errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()

if ($module -eq [IntPtr]::Zero) {
    throw ("LoadLibraryExW failed: error={0} path={1}" -f $errorCode, $fullPath)
}

$factoryAddress = [NativePluginLoadTest]::GetProcAddress(
    $module, 'GetL4NPluginInstance')
if ($factoryAddress -eq [IntPtr]::Zero) {
    throw 'GetProcAddress(GetL4NPluginInstance) failed.'
}

$factory = [Runtime.InteropServices.Marshal]::GetDelegateForFunctionPointer(
    $factoryAddress, [NativePluginLoadTest+Factory])
$instance = $factory.Invoke()
if ($instance -eq [IntPtr]::Zero) {
    throw 'GetL4NPluginInstance returned null.'
}

Write-Host ("LoadLibraryExW OK: module=0x{0:X8}" -f $module.ToInt32()) -ForegroundColor Green
Write-Host ("GetL4NPluginInstance OK: factory=0x{0:X8} instance=0x{1:X8}" -f
    $factoryAddress.ToInt32(), $instance.ToInt32()) -ForegroundColor Green
