from os.path import isfile


Import('env')

DEFAULT_FILE = '.env.local'
EXAMPLE_FILE = '.env.example'
ENV_FILE = DEFAULT_FILE if isfile(DEFAULT_FILE) else EXAMPLE_FILE

assert isfile(ENV_FILE), f'Missing environ configuration file.'

with open(ENV_FILE, 'r') as env_file:
	env.Append(BUILD_FLAGS=[
		f'-D{line.strip()}'
		for line in env_file.readlines()
		if not line.startswith('#')
	])
