FROM python:3.11-slim

RUN apt-get update && apt-get install -y \
    libgl1-mesa-glx \
    libglib2.0-0 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY ozayn/ml/requirements.txt requirements.txt
RUN pip install --no-cache-dir -r requirements.txt

COPY ozayn/ml/ /app/

RUN mkdir -p /app/models /app/data

EXPOSE 8765

CMD ["python", "server.py"]
