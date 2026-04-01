int abs(int x) {
  if (x < 0) {
    return -x;
  }
  return x;
}

int max(int a, int b) {
  if (a > b) {
    return a;
  } else {
    return b;
  }
}

int clamp(int x, int lo, int hi) {
  if (x < lo) {
    return lo;
  } else if (x > hi) {
    return hi;
  } else {
    return x;
  }
}

int sign(int x) {
  if (x > 0) {
    return 1;
  } else if (x < 0) {
    return -1;
  } else {
    return 0;
  }
}

int main() {
  return 0;
}
