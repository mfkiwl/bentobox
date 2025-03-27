int tolower(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

int isdigit(int c) {
    return (c >= '0' && c <= '9');
}