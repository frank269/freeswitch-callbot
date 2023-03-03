FROM python:latest
LABEL Maintainer="tien2609"
WORKDIR /usr/app/src
COPY . ./
RUN pip install -r requirements.txt
EXPOSE 9000 
CMD [ "python", "./server.py"]