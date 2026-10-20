<#
****************************************
*
* Elevator Lab -- Direct Network Firewall Configuration
*
* SPDX-License-Identifier: MIT
*
****************************************
#>

#Requires -RunAsAdministrator
param(
    [ValidateRange(1024, 65535)][int]$Port = 8099,
    [string]$Program,
    [string]$ResultPath
)
$ErrorActionPreference = 'Stop'
try {
    # Match the 5G and SDV dashboards: open one TCP port for all profiles.
    # RULE SCOPE: Resolve an optional executable restriction or allow the chosen TCP port for any program
    $programPath = if ($Program) { (Resolve-Path -LiteralPath $Program).Path } else { 'Any' }
    $ruleName = "ElevatorLab-TCP-$Port"
    $rule = Get-NetFirewallRule -Name $ruleName -ErrorAction SilentlyContinue
    # CONFIGURATION: Update the named rule if present; otherwise create that one rule
    if ($rule) {
        $rule | Set-NetFirewallRule -Enabled True -Direction Inbound -Action Allow -Profile Any -Protocol TCP -LocalPort $Port -RemotePort Any -LocalAddress Any -RemoteAddress Any -Program $programPath
    } else {
        New-NetFirewallRule -Name $ruleName -DisplayName "Elevator Lab (TCP $Port)" -Enabled True -Direction Inbound -Action Allow -Profile Any -Protocol TCP -LocalPort $Port -RemotePort Any -LocalAddress Any -RemoteAddress Any -Program $programPath | Out-Null
    }
    # VERIFICATION: Read the active rule and filters rather than assuming configuration succeeded
    $verified = Get-NetFirewallRule -Name $ruleName -PolicyStore ActiveStore
    $portFilter = $verified | Get-NetFirewallPortFilter
    $appFilter = $verified | Get-NetFirewallApplicationFilter
    if ($verified.Enabled -ne 'True' -or $verified.Action -ne 'Allow' -or $portFilter.LocalPort -ne "$Port" -or $appFilter.Program -ne $programPath) {
        throw 'Firewall rule did not become active with the expected port and program.'
    }
    # RESULT: Return machine-readable success metadata and optionally save it to the requested path
    $result = [ordered]@{ Success = $true; Rule = $ruleName; Port = $Port; Program = $programPath; Profile = "$($verified.Profile)" }
    if ($ResultPath) { $result | ConvertTo-Json | Set-Content -LiteralPath $ResultPath }
    $result | ConvertTo-Json | Write-Output
} catch {
    if ($ResultPath) { @{ Success = $false; Error = $_.Exception.Message } | ConvertTo-Json | Set-Content -LiteralPath $ResultPath }
    throw
}
