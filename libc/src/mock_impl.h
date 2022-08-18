static long double strtox(const char *s, char **p, int prec) { return 0; }

float strtof(const char *restrict s, char **restrict p) {
  return strtox(s, p, 0);
}

double strtod(const char *restrict s, char **restrict p) {
  return strtox(s, p, 1);
}

long double strtold(const char *restrict s, char **restrict p) {
  return strtox(s, p, 2);
}

double pow(double x, double y)
{
  return 0;
}
