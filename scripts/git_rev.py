#
# PlatformIO pre-build hook: inject a unique build identifier as -D GIT_REV.
#
# The reported fw_ver is a human semantic version and is intentionally reused
# across builds; GIT_REV disambiguates the EXACT build on the wire (backend asked
# for this — two different builds were reporting the same version string). Shape:
#   <git-describe-or-shorthash>[-dirty]@<branch>   e.g. "a30358a-dirty@new_mqtt_TB_with_modbus"
# Falls back gracefully if git is unavailable so the build never breaks.
#
import subprocess
Import("env")

def _git(args, fallback):
    try:
        return subprocess.check_output(["git"] + args,
                                       stderr=subprocess.DEVNULL).strip().decode()
    except Exception:
        return fallback

rev    = _git(["describe", "--always", "--dirty", "--tags"], "nogit")
branch = _git(["rev-parse", "--abbrev-ref", "HEAD"], "?")
build_id = "%s@%s" % (rev, branch)
print("git_rev.py -> GIT_REV = " + build_id)
env.Append(CPPDEFINES=[("GIT_REV", env.StringifyMacro(build_id))])
