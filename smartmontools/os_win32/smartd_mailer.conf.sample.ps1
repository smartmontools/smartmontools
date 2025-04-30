# Sample file for smartd_mailer.conf.ps1
#
# Home page of code is: http://www.smartmontools.org
# $Id$

# SMTP Server
$smtpServer = "smtp.domain.local"

# Optional settings [default values in square brackets]

# Sender address ["smartd daemon <root@$hostname>"]
#$from = "Administrator <root@domain.local>"

# SMTP Port [25]
#$port = 587

# Use STARTTLS [$false]
#$useSsl = $true

# SMTP user name []
#$username = "USER"

# Plain text SMTP password []
#$password = "PASSWORD"

# Encrypted SMTP password []
# (embedded newlines, tabs and spaces are ignored)
#$passwordEnc = "
#  0123456789abcdef...
#  ...
#"

# # -------------------------------------------------------------------------
# # Sample encryption/decryption for $password <-> $passwordEnc
# # (Requires same system and user account as smartd service)
#
# $password = "PASSWORD"
# Clear-History # Recommended if run interactively
# $t = ConvertTo-SecureString -AsPlainText -Force $password
# $t = ConvertFrom-SecureString $t
# $t = ($t -split '(.{78})' | Where-Object {$_}) -replace '^','  ' -join "`n"
# $passwordEnc = "`n" + $t + "`n"
# echo "`$passwordEnc = `"$passwordEnc`""
#
# $t = $passwordEnc -replace '[\r\n\t ]',''
# $t = ConvertTo-SecureString $t
# $t = [System.Runtime.InteropServices.Marshal]::SecureStringToBSTR($t)
# $password = [System.Runtime.InteropServices.Marshal]::PtrToStringAuto($t)
# $t = $undef
# echo "`$password = `"$password`""
#
# # -------------------------------------------------------------------------
