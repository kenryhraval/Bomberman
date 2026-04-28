FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y \
    gcc \
    make \
    libncursesw5-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN make server


FROM debian:bookworm-slim

WORKDIR /app

COPY --from=build /app/build/server_app /app/server_app
COPY --from=build /app/maps /app/maps

EXPOSE 6969

CMD ["./server_app"]