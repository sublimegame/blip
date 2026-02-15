$envFile = "$PSScriptRoot\..\.env"
$outFile = "$PSScriptRoot\envvars.h"

"// Auto-generated from .env" | Out-File $outFile -Encoding UTF8
foreach ($line in Get-Content $envFile) {
    if ($line -match '^\s*#') { continue } # skip comments
    if ($line -match '^\s*$') { continue } # skip empty lines
    if ($line -match '^(?<key>[^=]+)=(?<val>.+)$') {
        $k = $matches['key'].Trim()
        $v = $matches['val'].Trim()
		
        "#define $k $v" | Out-File $outFile -Append -Encoding UTF8
    }
}
