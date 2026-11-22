param([string]$Bundle,[string]$Tester,[string]$Destination="results/factory")
$ErrorActionPreference="Stop"
New-Item -ItemType Directory -Force $Destination | Out-Null
$culture=[System.Globalization.CultureInfo]::InvariantCulture
$rows=[System.Collections.Generic.List[object]]::new()
function RunTone([string]$group,[string]$label,[int]$ct,[int]$cr,[int]$rec,[int]$makeup,[int]$lt,[int]$lr,[int]$ci,[int]$li,[int]$fast,[int]$attack,[double]$hz,[double]$input,[double]$duration=1.2) {
  $argsTone=@($ct,$cr,$rec,$makeup,$lt,$lr,$ci,$li,$fast,$attack,$hz,0,$input,0,$duration,44100)
  $spectrum=@(& $Tester $Bundle --jd-case @argsTone 2> "$Destination/current-stderr.txt")
  if ($LASTEXITCODE -ne 0 -or $spectrum.Count -ne 20) {throw "Failed measurement $group/$label : $LASTEXITCODE (lines $($spectrum.Count))"}
  $magnitudes=@()
  foreach($line in $spectrum) {
    $parts=$line.Trim() -split '\s+'
    $re=[double]::Parse($parts[0],$culture);$im=[double]::Parse($parts[1],$culture)
    $magnitudes += [math]::Sqrt($re*$re+$im*$im)
  }
  if ($magnitudes[0] -le 0) {throw "Zero fundamental $label"}
  $power=0.0
  for($k=1;$k -lt [Math]::Min(20,[int][Math]::Floor(20000/$hz));$k++) {$power+=[math]::Pow($magnitudes[$k],2)}
  $output=24+20*[math]::Log10($magnitudes[0])
  $thd=100*[math]::Sqrt($power)/$magnitudes[0]
  $rows.Add([pscustomobject]@{Group=$group; Case=$label; Input_dBu=$input; Frequency_Hz=$hz; Compressor=$ci; Limiter=$li; CompressorThresholdDetent=$ct; RatioDetent=$cr; CompressorRecoveryDetent=$rec; MakeupDetent=$makeup; LimiterThresholdDetent=$lt; LimiterRecoveryDetent=$lr; FastLimiter=$fast; CompressorAttackDetent=$attack; Output_dBu=[math]::Round($output,6); THD_percent=[math]::Round($thd,7); K2_dBc=[math]::Round(20*[math]::Log10([math]::Max(1e-12,$magnitudes[1]/$magnitudes[0])),4); K3_dBc=[math]::Round(20*[math]::Log10([math]::Max(1e-12,$magnitudes[2]/$magnitudes[0])),4)})
  return $output
}
$report=[System.Collections.Generic.List[object]]::new()
# Factory compressor ratios, 0->+10 dBu, -20 dBu threshold, 100ms recovery, 1kHz
$targets=@(6.5,5.0,3.5,2.5,1.5);$tolerances=@(1,1,1,.5,.5);$ratioNames=@("1.5:1","2:1","3:1","4:1","6:1")
for($r=0;$r -lt 5;$r++) {
  $lo=RunTone "ratio" "$($ratioNames[$r])-0" 0 $r 0 0 8 0 1 0 0 0 1000 0
  $hi=RunTone "ratio" "$($ratioNames[$r])-10" 0 $r 0 0 8 0 1 0 0 0 1000 10
  $change=$hi-$lo;$target=$targets[$r];$tol=$tolerances[$r]
  $report.Add([pscustomobject]@{Test="Factory compressor ratio $($ratioNames[$r])";Measured=[math]::Round($change,4);Nominal=$target;Tolerance=$tol;Result=$(if([math]::Abs($change-$target) -le $tol){"PASS"}else{"FAIL"})})
}
# Makeup gain is relative to the same compressed tone with zero makeup (20 dB gain range)
$baseline=RunTone "makeup" "base0" 10 2 0 0 8 0 1 0 0 0 1000 0
for($g=1;$g -le 10;$g++) {
  $out=RunTone "makeup" "$($g*2)dB" 10 2 0 $g 8 0 1 0 0 0 1000 0
  $actual=$out-$baseline;$target=2*$g
  $report.Add([pscustomobject]@{Test="Makeup $target dB";Measured=[math]::Round($actual,4);Nominal=$target;Tolerance=0.5;Result=$(if([math]::Abs($actual-$target) -le .5){"PASS"}else{"FAIL"})})
}
# Limiter factory alignment: +20 dBu input, +8 threshold, 50 ms recovery -> +8 dBu output.
# Limiter index 8 = +8 dBu. 0=50 ms recovery.
$aligned=RunTone "limiter" "alignment-input20-threshold8" 0 0 0 0 8 0 0 1 0 0 1000 20
$report.Add([pscustomobject]@{Test="Limiter threshold +8 alignment";Measured=[math]::Round($aligned,4);Nominal=8;Tolerance="reference adjustment";Result="OBSERVE"})
# Limiter ratio: +10 to +20 dBu; report both threshold choices, because limit action at +10 matters.
foreach($lt in @(0,8,22)) {
  $lo=RunTone "limiter" "ratio-thr$lt-input10" 0 0 0 0 $lt 0 0 1 0 0 1000 10
  $hi=RunTone "limiter" "ratio-thr$lt-input20" 0 0 0 0 $lt 0 0 1 0 0 1000 20
  $delta=$hi-$lo
  $report.Add([pscustomobject]@{Test="Limiter +10/+20 ratio threshold detent $lt";Measured=[math]::Round($delta,4);Nominal=0.1;Tolerance=0.1;Result=$(if($delta -ge 0 -and $delta -le .2){"PASS"}else{"FAIL"})})
}
# Limiter threshold sweep at +20 dBu source and 50 ms recovery.
foreach($lt in 0,2,4,6,8,10,12,14,16,18,20,22) {
  $out=RunTone "limiter-level" "limit-detent$lt" 0 0 0 0 $lt 0 0 1 0 0 1000 20
  $nominal=4+.5*$lt
  $report.Add([pscustomobject]@{Test="Limiter threshold $nominal dBu at +20 input";Measured=[math]::Round($out,4);Nominal=$nominal;Tolerance="characterization";Result="OBSERVE"})
}
# Frequency response: factory 0 dBu, relative to 1kHz, dynamics out.
$ref=RunTone "frequency" "1000Hz" 0 0 0 0 8 0 0 0 0 0 1000 0
foreach($hz in @(20,30,50,100,250,500,2000,5000,8000,10000,12000,15000,18000,20000)) {
  $out=RunTone "frequency" "$($hz)Hz" 0 0 0 0 8 0 0 0 0 0 $hz 0
  $response=$out-$ref
  $report.Add([pscustomobject]@{Test="Through frequency $hz Hz";Measured=[math]::Round($response,4);Nominal=0;Tolerance=0.5;Result=$(if([math]::Abs($response) -le .5){"PASS"}else{"FAIL"})})
}
# Factory THD, note no legacy -18 dBu limiter threshold detent exists in the plugin;
# use +8 limiter detent, and mark as a different factory-condition proxy.
$null=RunTone "thd" "through-plus9" 0 0 0 0 8 3 0 0 0 0 1000 9
$null=RunTone "thd" "compressor-6to1-plus9" 1 4 2 10 8 3 1 0 0 0 1000 9
$null=RunTone "thd" "limiter-plus22-threshold8" 0 0 0 0 8 3 0 1 0 0 1000 22
foreach($spec in @(@("through-plus9",.075,"Factory bypass THD"),@("compressor-6to1-plus9",.2,"Factory compressor settings THD, +9 dBu probe"),@("limiter-plus22-threshold8",.45,"Limiter +22 dBu THD, threshold +8 proxy"))) {
  $item=$rows | Where-Object {$_.Case -eq $spec[0]}
  $report.Add([pscustomobject]@{Test=$spec[2];Measured=$item.THD_percent;Nominal="< $($spec[1])%";Tolerance="MAX";Result=$(if($item.THD_percent -le [double]$spec[1]){"UNDER LIMIT"}else{"OVER LIMIT"})})
}
$rows | Export-Csv "$Destination/factory-raw-measurements.csv" -NoTypeInformation
$report | Export-Csv "$Destination/factory-summary.csv" -NoTypeInformation
$report | Format-Table -AutoSize | Out-String -Width 220 | Tee-Object "$Destination/factory-report.txt"
Write-Host "Factory suite cases: $($rows.Count) tones; report rows: $($report.Count)"
