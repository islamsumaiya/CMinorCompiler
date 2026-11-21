int main(void) {
    int a = 10;

    {
        int b = 20;
    }

    int b = 30;

    return a + b;
}