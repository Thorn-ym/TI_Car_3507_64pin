$ErrorActionPreference = 'Stop'

$inputPath = (Resolve-Path -LiteralPath 'output\设计报告核心代码摘录.docx').Path
$outputDir = Join-Path (Resolve-Path -LiteralPath 'report_work').Path 'code_appendix_word_render'
$pdfPath = Join-Path $outputDir '设计报告核心代码摘录.pdf'

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

$word = $null
$document = $null
try {
    $word = New-Object -ComObject Word.Application
    $word.Visible = $false
    $word.DisplayAlerts = 0
    $document = $word.Documents.Open($inputPath, $false, $true)
    $document.ExportAsFixedFormat($pdfPath, 17)
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
