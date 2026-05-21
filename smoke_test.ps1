Add-Type -TypeDefinition '
using System;
using System.Runtime.InteropServices;
using System.Text;

public class Win32 {
    [DllImport("user32.dll", SetLastError = true)]
    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);

    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern IntPtr SendMessageTimeout(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam, uint fuFlags, uint uTimeout, out IntPtr lpdwResult);

    [DllImport("user32.dll")]
    public static extern int GetDlgCtrlID(IntPtr hwnd);

    [DllImport("user32.dll")]
    public static extern bool EnumChildWindows(IntPtr hWndParent, EnumWindowsProc lpEnumFunc, IntPtr lParam);

    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
}
'

function Wait-ForWindow {
    param([string]$title, [int]$timeoutSeconds = 30)
    $start = Get-Date
    while ((Get-Date) -lt $start.AddSeconds($timeoutSeconds)) {
        $hwnd = [Win32]::FindWindow($null, $title)
        if ($hwnd -ne [IntPtr]::Zero) {
            $res = [IntPtr]::Zero
            # Msg 0 is WM_NULL
            $ret = [Win32]::SendMessageTimeout($hwnd, 0, [IntPtr]::Zero, [IntPtr]::Zero, 2, 1000, [ref]$res)
            if ($ret -ne [IntPtr]::Zero) { return $hwnd }
        }
        Start-Sleep -Milliseconds 500
    }
    return $null
}

function Get-ChildControls {
    param([IntPtr]$parentHwnd)
    $controls = New-Object System.Collections.Generic.List[IntPtr]
    $proc = {
        param($hwnd, $lparam)
        $controls.Add($hwnd)
        return $true
    }
    [Win32]::EnumChildWindows($parentHwnd, $proc, [IntPtr]::Zero)
    return $controls
}

try {
    Write-Host "Launching BasicGameEngine..."
    $proc = Start-Process "BasicGameEngine\x64\Debug\BasicGameEngine.exe" -ArgumentList "--session ui-history-smoke3 --launch-basic-game --ui-target ""Scene And Render"" --hide-workers" -PassThru

    Write-Host "Waiting for Controller window..."
    $controllerHwnd = Wait-ForWindow "BasicGameEngine - Controller"
    if (-not $controllerHwnd) { throw "Controller window not found" }
    Write-Host "Controller HWND: $controllerHwnd"

    Write-Host "Waiting for Worker window..."
    $workerHwnd = Wait-ForWindow "BasicGameEngine - Worker: bge.game-loop"
    if (-not $workerHwnd) { throw "Worker window not found" }
    Write-Host "Worker HWND: $workerHwnd"

    Write-Host "Verifying controls..."
    $children = Get-ChildControls $controllerHwnd
    $ids = $children | ForEach-Object { [Win32]::GetDlgCtrlID($_) }
    
    $targetIds = @(42300, 42301, 42902, 42903, 42400, 42401, 42402, 42403, 42404, 42405)
    foreach ($id in $targetIds) {
        if ($ids -notcontains $id) { throw "Control $id not found" }
    }
    Write-Host "All controls found."

    Add-Type -AssemblyName UIAutomationClient
    Add-Type -AssemblyName UIAutomationTypes

    $aeController = [System.Windows.Automation.AutomationElement]::FromHandle($controllerHwnd)
    
    $soundButton = $aeController.FindFirst([System.Windows.Automation.TreeScope]::Descendants, (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, "42405")))
    $historyList = $aeController.FindFirst([System.Windows.Automation.TreeScope]::Descendants, (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, "42902")))
    $detailText = $aeController.FindFirst([System.Windows.Automation.TreeScope]::Descendants, (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, "42903")))
    
    $initialItems = $historyList.FindAll([System.Windows.Automation.TreeScope]::Children, [System.Windows.Automation.Condition]::TrueCondition)
    $initialCount = $initialItems.Count
    Write-Host "Initial history count: $initialCount"
    if ($initialCount -lt 1) { throw "Initial history count < 1" }

    Write-Host "Clicking Sound target..."
    $invokePattern = $soundButton.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
    $invokePattern.Invoke()
    Start-Sleep -Seconds 1

    $countAfterSound = $historyList.FindAll([System.Windows.Automation.TreeScope]::Children, [System.Windows.Automation.Condition]::TrueCondition).Count
    Write-Host "Count after Sound: $countAfterSound"
    if ($countAfterSound -le $initialCount) { throw "History count did not increase after sound click" }

    $textPattern = $detailText.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    $detailVal = $textPattern.Current.Value
    Write-Host "Detail: $detailVal"
    if ($detailVal -notlike "*Target -> Audio / Sound*") { throw "Detail does not mention sound" }

    Write-Host "Running custom command..."
    $edit42300 = $aeController.FindFirst([System.Windows.Automation.TreeScope]::Descendants, (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, "42300")))
    $editValue = $edit42300.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    $editValue.SetValue("game-loop: add 1")
    
    $runButton = $aeController.FindFirst([System.Windows.Automation.TreeScope]::Descendants, (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty, "42301")))
    $runInvoke = $runButton.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
    $runInvoke.Invoke()

    Write-Host "Waiting for log markers..."
    $logPath = "BasicGameEngine.log"
    $foundLog = $false
    for ($i=0; $i -lt 10; $i++) {
        if (Test-Path $logPath) {
             $lines = Get-Content $logPath -Tail 20
             $hasBtn = $lines | Where-Object { $_ -like "*[BgeControllerCommand]*target=bge.game-loop*command=""add 1""*result=ok*" }
             $hasWrk = $lines | Where-Object { $_ -like "*[BgeWorkerCommand]*role=bge.game-loop*command=""add 1""*result=ok*" }
             if ($hasBtn -and $hasWrk) {
                 $foundLog = $true
                 Write-Host "Log markers found:"
                 Write-Host ($hasBtn | Select-Object -Last 1)
                 Write-Host ($hasWrk | Select-Object -Last 1)
                 break
             }
        }
        Start-Sleep -Seconds 1
    }
    if (-not $foundLog) { throw "Log markers not found" }

    $finalCount = $historyList.FindAll([System.Windows.Automation.TreeScope]::Children, [System.Windows.Automation.Condition]::TrueCondition).Count
    Write-Host "Final count: $finalCount"
    if ($finalCount -le $countAfterSound) { throw "History count did not increase after run" }

    $detailValFinal = $textPattern.Current.Value
    Write-Host "Final Detail: $detailValFinal"
    if ($detailValFinal -notlike "*Input -> game-loop: add 1*" -and $detailValFinal -notlike "*Command OK -> Game Loop: add 1*") {
        throw "Final detail mismatch"
    }

    Write-Host "SMOKE TEST PASSED"
} catch {
    Write-Host "SMOKE TEST FAILED: $(.Exception.Message)"
    exit 1
} finally {
    Write-Host "Cleaning up processes..."
    Get-WmiObject Win32_Process | Where-Object { $_.CommandLine -like "*--session ui-history-smoke3*" } | ForEach-Object { 
        Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
    }
}
