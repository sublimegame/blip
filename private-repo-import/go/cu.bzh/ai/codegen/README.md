# How to run

## Step 1

Launch ChromaDB

```bash
docker run -d -p 9999:8000 --name chromadb chromadb/chroma:0.5.20
```

## Step 2

Launch Ollama

## Step 3

Pull models in ollama

```bash
ollama pull llama3.2:3b
ollama pull mxbai-embed-large
```
