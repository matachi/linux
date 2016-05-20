#!/usr/bin/env python3
import shlex, subprocess
import os
import time
import re
import sys


env = os.environ.copy()
env['SRCARCH'] = 'x86'
env['ARCH'] = 'x86'
env['KERNELVERSION'] = '4.4.10'


def communicate(p):
    return map(
        lambda x: x.decode('utf-8') if x is not None else None,
        p.communicate())


def get_random_options(num):
    args = shlex.split('scripts/kconfig/rangefixconfig Kconfig random %i' % num)
    p = subprocess.Popen(args, env=env, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
    out, err = communicate(p)
    return list(filter(None, out.split('\n')))


re_diagnosis = re.compile(r'Diagnosis: (.*)\n')
re_full_diagnosis = re.compile(r'Full diagnosis: (.*)\n')
re_total = re.compile(r'Total: (\d+) ms')
re_setup = re.compile(r'Setup: (\d+) ms')
re_find = re.compile(r'Find: (\d+) ms')
re_simplify = re.compile(r'Simplify: (\d+) ms')
re_iterations = re.compile(r'Iterations: (\d+)')


def do_iteration(option_name):
    args = shlex.split('scripts/kconfig/rangefixconfig %s YES' % option_name)
    p = subprocess.Popen(args, env=env, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)

    time_until_timeout = 40

    while time_until_timeout > 0:
        time.sleep(1)
        time_until_timeout -= 1
        if p.poll() is None:
            continue
        out, err = communicate(p)
        print('\n## out ##')
        print(out)
        print('\n## err ##')
        print(err)
        diagnoses = [
                list(filter(None, d.split(', ',)))
                for d in re_diagnosis.findall(out)]
        full_diagnoses = [
                list(filter(None, d.split(', ',)))
                for d in re_full_diagnosis.findall(out)]
        total = re_total.search(out).group(1)
        setup = re_setup.search(out).group(1)
        find = re_find.search(out).group(1)
        simplify = re_simplify.search(out).group(1)
        iterations = re_iterations.search(out).group(1)
        return total, setup, find, simplify, iterations, diagnoses, \
               full_diagnoses
    p.kill()
    return None


output = []
for i, option in enumerate(get_random_options(200), 1):
    print('### {} {} ###'.format(i, option))
    print(i, option, file=sys.stderr)
    sys.stdout.flush()
    data = do_iteration(option)
    output.append((option, data))

print('\n### OUTPUT ###\n')
print(output)

print('\n### RESULTS ###\n')
for row in output:
    option, data = row
    print(option)
    if data is None:
        print('timeout')
    else:
            total, setup, find, simplify, iterations, diagnoses, \
                full_diagnoses = data
            print(
                'Total: {} ms, Setup: {} ms, Find: {} ms, Simplify: {} ms, '
                'Iterations: {}'.format(
                    total, setup, find, simplify, iterations))
            print('Diagnoses')
            for diagnosis in diagnoses:
                print(diagnosis)
            print('Full diagnoses')
            for full_diagnosis in full_diagnoses:
                print(full_diagnosis)
    print()
