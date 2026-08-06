$ErrorActionPreference = 'Stop'

$sourcePath = (Resolve-Path -LiteralPath 'output\设计报告核心代码摘录.docx').Path
$outputDir = Join-Path (Resolve-Path -LiteralPath 'report_work').Path 'code_appendix_word_render'
$tempDir = Join-Path $env:TEMP 'codex_code_appendix_render'
$inputPath = Join-Path $tempDir 'code_appendix.docx'
$tempPdfPath = Join-Path $tempDir 'code_appendix.pdf'
$pdfPath = Join-Path $outputDir 'code_appendix.pdf'

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
New-Item -ItemType Directory -Force -Path $tempDir | Out-Null
Copy-Item -LiteralPath $sourcePath -Destination $inputPath -Force

$word = $null
$document = $null
try {
    $word = New-Object -ComObject Word.Application
    $word.Visible = $false
    $word.DisplayAlerts = 0
    $document = $word.Documents.OpenNoRepairDialog($inputPath, $false, $true)
    $document.ExportAsFixedFormat($tempPdfPath, 17)
    Copy-Item -LiteralPath $tempPdfPath -Destination $pdfPath -Force
}
finally {
    if ($null -ne $document) {
        $document.Close($false)
        [void][Runtime.InteropServices.Marshal]::ReleaseComObject($document)
    }
    if ($null -ne $word) {
        $word.Quit()
        [void][Runtime.InteropServices.Marshal]::ReleaseComObject($word)
    }
    [GC]::Collect()
    [GC]::WaitForPendingFinalizers()
}

Get-Item -LiteralPath $pdfPath | Select-Object FullName, Length, LastWriteTime
