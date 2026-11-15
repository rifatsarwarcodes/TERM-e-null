# TERM-e-null (OV_C)

OV_C is a terminal-based exam environment for C++. Students write, compile
and test their code in a terminal editor while OV_C records how the solution
was written. Examiners review the record in a terminal dashboard.

The project is built for SWE 4304 (Software Project Lab I) at the Islamic
University of Technology.

## Building

```
make          # builds build/ovc
make test     # builds and runs the unit tests
```

On Windows with MinGW-w64 from a normal command prompt, use `build.bat`
(or `build.bat test`).

The code is C++17 and uses only the standard library and the operating
system's console/process APIs.
