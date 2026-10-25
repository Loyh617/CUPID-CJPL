"""Small end-to-end checks using the actual Geant4 executable, without third-party packages."""
import itertools
import pathlib
import re
import subprocess
import sys

executable = pathlib.Path(sys.argv[1]).resolve()
output = pathlib.Path(sys.argv[2]).resolve()
output.mkdir(parents=True, exist_ok=True)
passed = 0


def run(name, commands, arguments=('--serial',), expected=None, fail=False):
    global passed
    macro = output / (name + '.mac')
    # Retired 2026-10-21: actions are placeholders; use native run summaries.
    # Restore only if CUPID_RUN_SUMMARY is reintroduced.
    # macro.write_text('/random/setSeeds 12345 67890\n' + commands + '\n', encoding='utf-8')
    macro.write_text('/random/setSeeds 12345 67890\n/run/verbose 1\n' + commands + '\n', encoding='utf-8')
    completed = subprocess.run([str(executable), str(macro), *arguments], cwd=output,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, timeout=45)
    log = completed.stdout
    (output / (name + '.log')).write_text(log, encoding='utf-8')
    if fail:
        assert completed.returncode != 0, f'{name}: invalid input was accepted'
    else:
        assert completed.returncode == 0, f'{name}: exit {completed.returncode}; see {name}.log'
        assert 'GeomVol1002' not in log and 'GeomNav' not in log, f'{name}: geometry warning'
        assert 'result=FAIL' not in log, f'{name}: overlap failure'
        assert 'Run Aborted after' not in log and 'Event Must Be Aborted' not in log, f'{name}: aborted transport'
    if expected:
        assert re.search(expected, log), f'{name}: missing {expected!r}; see {name}.log'
    passed += 1
    print('PASS', name, flush=True)


components = ('Cryostat', 'InnerShielding', 'LMOArray')
# Test all supported compositions, including a deliberately empty world.
for index, flags in enumerate(itertools.product((False, True), repeat=3)):
    settings = '\n'.join('/cupid/geometry/enable ' + name
                         for name, enabled in zip(components, flags) if enabled)
    run(f'geometry_{index}', settings + '\n/run/initialize\n/cupid/geometry/check 3000',
        expected=r'CUPID_GEOMETRY_CHECK .*result=PASS')

full = '\n'.join('/cupid/geometry/enable ' + c for c in components)
transport = ('\n/run/initialize\n/gun/particle gamma\n/gun/energy 1 MeV'
             '\n/gun/position 0 -350 -100 mm\n/gun/direction 0 0 1\n/run/beamOn 100')
# Retired 2026-10-21: actions are placeholders; use native run summaries.
# Restore only if CUPID_RUN_SUMMARY is reintroduced.
# run('transport_serial', full + transport, expected=r'CUPID_RUN_SUMMARY run=0 events=100 aborted=0')
# run('transport_mt', full + transport, arguments=('--threads', '2'),
#     expected=r'CUPID_RUN_SUMMARY run=0 events=100 aborted=0')
run('transport_serial', full + transport, expected=r'(?m)^  Number of events processed : 100$')
run('transport_mt', full + transport, arguments=('--threads', '2'),
    expected=r'(?m)^  Number of events processed : 100$')
run('two_lmos', full + '\n/cupid/geometry/lmoCount 2\n/run/initialize'
    '\n/cupid/geometry/check 10000', expected=r'CUPID_LMO id=1 position_mm=')
run('disable_component', '/cupid/geometry/enable Cryostat\n/cupid/geometry/disable Cryostat'
    '\n/run/initialize\n/cupid/geometry/check 1000',
    expected=r'CUPID_GEOMETRY_CHECK volumes=0 .*result=PASS')
run('invalid_component', '/cupid/geometry/enable DoesNotExist', fail=True,
    expected=r'Unknown geometry component')
run('invalid_count', '/cupid/geometry/lmoCount 0', fail=True)
run('late_change', '/run/initialize\n/cupid/geometry/enable Cryostat', fail=True)
run('unknown_command', '/cupid/doesNotExist', fail=True)
run('nested_failure', '/control/execute invalid_component.mac', fail=True)
# Retired 2026-10-21: actions are placeholders; use native run summaries.
# Restore only if CUPID_RUN_SUMMARY is reintroduced.
# run('repeated_run', '/cupid/geometry/enable LMOArray\n/run/initialize'
#     '\n/run/beamOn 3\n/run/beamOn 7', expected=r'CUPID_RUN_SUMMARY run=1 events=7 aborted=0')
run('repeated_run', '/cupid/geometry/enable LMOArray\n/run/initialize'
    '\n/run/beamOn 3\n/run/beamOn 7',
    expected=r'(?ms)^  Number of events processed : 3$.*^  Number of events processed : 7$')
# A charged test particle starts in LMO: the tracking log must show actual steps there.
run('lmo_transport', '/cupid/geometry/enable LMOArray\n/run/initialize'
    '\n/gun/particle e-\n/gun/energy 100 keV\n/gun/position 0 -350 0 mm'
    '\n/tracking/verbose 1\n/run/beamOn 1', expected=r'LMO0\b.*(?:eIoni|msc)')

missing = subprocess.run([str(executable), str(output/'missing.mac')],
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
assert missing.returncode != 0 and 'Macro file not found' in missing.stdout
passed += 1
if '--with-vis' in sys.argv[3:]:
    run('offscreen_render', '/control/execute render.mac', arguments=('--serial', '--vis'))
    assert (output/'geometry.png').read_bytes().startswith(b'\x89PNG\r\n\x1a\n')
print(f'All {passed} checks passed.', flush=True)
