# Nested `sere shell` only. Does not load the user profile.
# PATH and SERE_* are already set by the parent `sere` process.
function global:prompt {
  "(sere:$env:SERE_PROJECT_NAME) $($executionContext.SessionState.Path.CurrentLocation.ProviderPath)> "
}
function global:deactivate {
  Write-Host 'Leaving nested Sere shell.'
  exit
}
