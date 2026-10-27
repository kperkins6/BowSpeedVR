param(
    [Parameter(Mandatory=$true)][string]$SchemaFile,
    [Parameter(Mandatory=$true)][string]$ModuleConfig
)
$ErrorActionPreference='Stop'
$schemas=[System.Xml.Schema.XmlSchemaSet]::new()
$schemas.XmlResolver=[System.Xml.XmlUrlResolver]::new()
$schemas.Add('',(Resolve-Path -LiteralPath $SchemaFile).Path) | Out-Null
$schemas.Compile()
$settings=[System.Xml.XmlReaderSettings]::new()
$settings.Schemas=$schemas
$settings.ValidationType=[System.Xml.ValidationType]::Schema
$reader=[System.Xml.XmlReader]::Create((Resolve-Path -LiteralPath $ModuleConfig).Path,$settings)
try { while ($reader.Read()) {} } finally { $reader.Close() }
'PASS: FOMOD XML validates against the public 5.0 schema chain'
