int main() {
    int x;
    int y;
    char letra;

    x = 10;
    y = -20;
    letra = 'a';

    if (x <= y || !(x == 0)) {
        print("valor:\n");
    } else {
        print("fim\tok");
    }

    for (x = 0; x < 3; x = x + 1) {
        y = y - x;
    }

    return y;
}
