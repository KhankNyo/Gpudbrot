
gcc -Wextra -Wall \
    -I"./extern/glad/include/" \
    ./main.c ./extern/glad/src/glad.c \
    -o ./main \
    -lglfw
